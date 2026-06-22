#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   
#include <fcntl.h>    
#include <dirent.h>   
#include <pthread.h>
#include "common.h"
#include "db.h"

#define DB_PATH "./" 

void get_db_file_path(char* table_name, char* path_out) {
    snprintf(path_out, MAX_BUFFER, "%s%s.db", DB_PATH, table_name);
}

int set_posix_lock(int fd, short type) {
    struct flock fl;
    fl.l_type = type;     
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;       
    fl.l_len = 0;         
    return fcntl(fd, F_SETLKW, &fl); 
}

int exec_show_tables(char* result_out, char* error_payload) {
    DIR *d;
    struct dirent *dir;
    d = opendir(DB_PATH);
    result_out[0] = '\0';
    int found = 0;

    if (d) {
        strcat(result_out, "AVAILABLE TABLES:\n");
        while ((dir = readdir(d)) != NULL) {
            char *ext = strstr(dir->d_name, ".db");
            if (ext && ext == dir->d_name + strlen(dir->d_name) - 3) {
                char tname[256];
                strncpy(tname, dir->d_name, ext - dir->d_name);
                tname[ext - dir->d_name] = '\0';
                
                char line[300];
                snprintf(line, sizeof(line), "  -> %s\n", tname);
                strcat(result_out, line);
                found = 1;
            }
        }
        closedir(d);
        if (!found) strcat(result_out, "  (No tables exist yet)\n");
        return 1;
    } 
    else {
        strcpy(error_payload, "Internal Error: Could not open DB directory.");
        return 0;
    }
}

int exec_create(char* table_name, char* error_payload) {
    pthread_mutex_lock(&db_mutex);
    char file_path[MAX_BUFFER];
    get_db_file_path(table_name, file_path);
    int fd = open(file_path, O_WRONLY | O_CREAT | O_EXCL, 0644); 
    if (fd < 0) {
        strcpy(error_payload, "Table already exists or creation failed.");
        pthread_mutex_unlock(&db_mutex);
        return 0; 
    }
    close(fd); 
    pthread_mutex_unlock(&db_mutex);
    return 1; 
}

int exec_drop(char* table_name, char* error_payload) {
    pthread_mutex_lock(&db_mutex);
    char file_path[MAX_BUFFER];
    get_db_file_path(table_name, file_path);
    
    if (remove(file_path) == 0) {
        pthread_mutex_unlock(&db_mutex);
        return 1;
    } else {
        strcpy(error_payload, "Table not found or unable to drop.");
        pthread_mutex_unlock(&db_mutex);
        return 0;
    }
}

int exec_insert(char* table_name, Account *new_acc, char* error_payload) {
    pthread_mutex_lock(&db_mutex);
    char file_path[MAX_BUFFER];
    get_db_file_path(table_name, file_path);
    int fd = open(file_path, O_RDWR); 
    if (fd < 0) { strcpy(error_payload, "Table not found."); pthread_mutex_unlock(&db_mutex); return 0; }
    
    if (set_posix_lock(fd, F_WRLCK) < 0) { strcpy(error_payload, "DB busy."); close(fd); pthread_mutex_unlock(&db_mutex); return 0; }

    Account temp;
    while (read(fd, &temp, sizeof(Account)) == sizeof(Account)) {
        if (temp.acc_no == new_acc->acc_no) {
            strcpy(error_payload, "Primary Key Error: Account number already exists!");
            set_posix_lock(fd, F_UNLCK); close(fd);
            pthread_mutex_unlock(&db_mutex);
            return 0;
        }
    }
    sleep(5);
    write(fd, new_acc, sizeof(Account));
    set_posix_lock(fd, F_UNLCK); close(fd);
    pthread_mutex_unlock(&db_mutex);
    return 1; 
}

int exec_select(char* table_name, char* param, char* result_out, char* error_payload) {
    pthread_mutex_lock(&db_mutex); 
    char file_path[MAX_BUFFER];
    get_db_file_path(table_name, file_path);
    int fd = open(file_path, O_RDONLY); 
    if (fd < 0) { strcpy(error_payload, "Table not found."); pthread_mutex_unlock(&db_mutex); return 0; }
    
    if (set_posix_lock(fd, F_RDLCK) < 0) { strcpy(error_payload, "DB busy."); close(fd); pthread_mutex_unlock(&db_mutex); return 0; }

    int fetch_all = (strcmp(param, "*") == 0);
    int target_id = fetch_all ? 0 : atoi(param);
    int found = 0;
    char buffer[MAX_PAYLOAD];
    Account a;
    result_out[0] = '\0'; 
    
    while (read(fd, &a, sizeof(Account)) == sizeof(Account)) {
        if (fetch_all || a.acc_no == target_id) {
            snprintf(buffer, MAX_PAYLOAD, "  Acc: %-6d | Name: %-15s | Bal: $%-8.2f \n", a.acc_no, a.name, a.balance);
            strcat(result_out, buffer);
            found = 1;
        }
    }
    set_posix_lock(fd, F_UNLCK); close(fd);
    pthread_mutex_unlock(&db_mutex);

    if (!found) { strcpy(error_payload, "Record not found."); return 0; }
    return 1; 
}

int exec_update(char* table_name, int acc_no, float new_bal, char* error_payload) {
    pthread_mutex_lock(&db_mutex); 
    char file_path[MAX_BUFFER];
    get_db_file_path(table_name, file_path);
    int fd = open(file_path, O_RDWR); 
    if (fd < 0) { strcpy(error_payload, "Table not found."); pthread_mutex_unlock(&db_mutex); return 0; }
    
    if (set_posix_lock(fd, F_WRLCK) < 0) { strcpy(error_payload, "DB busy."); close(fd); pthread_mutex_unlock(&db_mutex); return 0; }

    Account a;
    int found = 0;
    while (read(fd, &a, sizeof(Account)) == sizeof(Account)) {
        if (a.acc_no == acc_no) {
            a.balance = new_bal;
            lseek(fd, -sizeof(Account), SEEK_CUR); 
	    sleep(5);
            write(fd, &a, sizeof(Account));        
            found = 1; 
            break;
        }
    }
    set_posix_lock(fd, F_UNLCK); close(fd);
    pthread_mutex_unlock(&db_mutex);

    if (!found) { strcpy(error_payload, "Account not found."); return 0; }
    return 1;
}

int exec_delete(char* table_name, int target_no, char* error_payload) {
    pthread_mutex_lock(&db_mutex);
    char file_path[MAX_BUFFER];
    get_db_file_path(table_name, file_path);
    int fd = open(file_path, O_RDWR); 
    if (fd < 0) { strcpy(error_payload, "Table not found."); pthread_mutex_unlock(&db_mutex); return 0; }
    
    if (set_posix_lock(fd, F_WRLCK) < 0) { strcpy(error_payload, "DB busy."); close(fd); pthread_mutex_unlock(&db_mutex); return 0; }

    Account temp_arr[1000]; 
    Account a;
    int count = 0, found = 0;

    while (read(fd, &a, sizeof(Account)) == sizeof(Account)) {
        if (a.acc_no == target_no) found = 1; 
        else temp_arr[count++] = a;           
    }

    if (found) {
        ftruncate(fd, 0); 
        lseek(fd, 0, SEEK_SET); 
        write(fd, temp_arr, count * sizeof(Account));
    }

    set_posix_lock(fd, F_UNLCK); close(fd);
    pthread_mutex_unlock(&db_mutex);

    if (!found) { strcpy(error_payload, "Account not found."); return 0; }
    return 1;
}
