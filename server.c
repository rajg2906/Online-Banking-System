#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "common.h"
#include "db.h"

int server_fd; 
int active_connections = 0; 
pthread_mutex_t conn_mutex; 
pthread_mutex_t db_mutex; 
int server_mq_id = -1; 

void handle_shutdown(int sig) {
    printf("\n[server] Shutting down gracefully...\n");
    if (server_mq_id != -1) msgctl(server_mq_id, IPC_RMID, NULL); 
    pthread_mutex_destroy(&conn_mutex);
    pthread_mutex_destroy(&db_mutex);
    close(server_fd);
    exit(0);
}

void* logger_thread_func(void* arg) {
    LogMsg log_buf;
    while(1) {
        if (msgrcv(server_mq_id, &log_buf, sizeof(log_buf.mtext), 1, 0) > 0) {
            printf("[LOG] %s\n", log_buf.mtext);
        }
    }
    return NULL;
}

void log_event(const char* event_desc) {
    if (server_mq_id == -1) return;
    LogMsg msg;
    msg.mtype = 1;
    strncpy(msg.mtext, event_desc, MAX_BUFFER - 1);
    msg.mtext[MAX_BUFFER - 1] = '\0';
    msgsnd(server_mq_id, &msg, sizeof(msg.mtext), 0);
}

int is_authorized(char* role, int cmd) {
    if (strcmp(role, ROLE_ADMIN) == 0) return 1; 
    if (strcmp(role, ROLE_USER) == 0) return (cmd == CMD_SELECT || cmd == CMD_INSERT || cmd == CMD_UPDATE || cmd == CMD_SHOW_TABLES || cmd == CMD_LOGOUT || cmd == CMD_EXIT || cmd == CMD_DELETE);
    if (strcmp(role, ROLE_GUEST) == 0) return (cmd == CMD_SELECT || cmd == CMD_SHOW_TABLES || cmd == CMD_LOGOUT || cmd == CMD_EXIT);
    return 0; 
}

void *handle_client_thread(void *arg) {
    int client_socket = *(int *)arg;
    char my_role[20] = ""; 
    Msg msg;
    int is_logged_in = 0;
    char log_buffer[MAX_BUFFER];

    while (1) {
        if (read(client_socket, &msg, sizeof(msg)) <= 0) break; 

        if (!is_logged_in) {
            if (msg.cmd == CMD_LOGIN) {
                char req_role[20], req_pass[50] = "";
                
                char *sep = strchr(msg.payload, ':');
                if (sep) {
                    *sep = '\0';
                    strcpy(req_role, msg.payload);
                    strcpy(req_pass, sep + 1);
                } else {
                    strcpy(req_role, msg.payload);
                }

                int auth_success = 0;
                if (strcmp(req_role, ROLE_ADMIN) == 0 && strcmp(req_pass, "admin123") == 0) auth_success = 1;
                else if (strcmp(req_role, ROLE_USER) == 0 && strcmp(req_pass, "user123") == 0) auth_success = 1;
                else if (strcmp(req_role, ROLE_GUEST) == 0) auth_success = 1;

                if (auth_success) {
                    strcpy(my_role, req_role);
                    is_logged_in = 1;
                    
                    snprintf(log_buffer, sizeof(log_buffer), "Client logged in as: %s", my_role);
                    log_event(log_buffer);

                    msg.cmd = CMD_LOGIN;
                    strcpy(msg.payload, "Login successful.");
                } else {
                    msg.cmd = CMD_ERROR;
                    strcpy(msg.payload, "Invalid credentials.");
                }
                write(client_socket, &msg, sizeof(msg));
            } else if (msg.cmd == CMD_EXIT) {
                goto cleanup;
            } else {
                msg.cmd = CMD_ERROR;
                strcpy(msg.payload, "Login required.");
                write(client_socket, &msg, sizeof(msg));
            }
            continue;
        }

        if (msg.cmd == CMD_LOGOUT) {
            snprintf(log_buffer, sizeof(log_buffer), "Client logged out: %s", my_role);
            log_event(log_buffer);
            
            is_logged_in = 0;
            my_role[0] = '\0';
            msg.cmd = CMD_LOGOUT;
            strcpy(msg.payload, "Successfully logged out.");
            write(client_socket, &msg, sizeof(msg));
            continue;
        }

        if (!is_authorized(my_role, msg.cmd)) {
            msg.cmd = CMD_ERROR;
            strcpy(msg.payload, "Operation Not Allowed for your role.");
            write(client_socket, &msg, sizeof(msg));
            continue; 
        }

        char t_name[50];
        Account a;

        switch (msg.cmd) {
            case CMD_EXIT: goto cleanup;
            case CMD_SHOW_TABLES:
                if (exec_show_tables(msg.payload, msg.payload)) msg.cmd = CMD_SHOW_TABLES; 
                else msg.cmd = CMD_ERROR;
                break;
            case CMD_CREATE:
                if (exec_create(msg.payload, msg.payload)) { msg.cmd = CMD_CREATE; strcpy(msg.payload, "Table created."); } 
                else msg.cmd = CMD_ERROR;
                break;
            case CMD_DROP:
                if (exec_drop(msg.payload, msg.payload)) { msg.cmd = CMD_DROP; strcpy(msg.payload, "Table dropped."); } 
                else msg.cmd = CMD_ERROR;
                break;
            case CMD_INSERT:
                sscanf(msg.payload, "%s %d %s %f", t_name, &a.acc_no, a.name, &a.balance);
                if (exec_insert(t_name, &a, msg.payload)) { msg.cmd = CMD_INSERT; strcpy(msg.payload, "Account added."); } 
                else msg.cmd = CMD_ERROR;
                break;
            case CMD_UPDATE:
                sscanf(msg.payload, "%s %d %f", t_name, &a.acc_no, &a.balance);
                if (exec_update(t_name, a.acc_no, a.balance, msg.payload)) { msg.cmd = CMD_UPDATE; strcpy(msg.payload, "Balance updated."); } 
                else msg.cmd = CMD_ERROR;
                break;
            case CMD_SELECT:
                {
                    char param[50], result[MAX_PAYLOAD];
                    sscanf(msg.payload, "%s %s", t_name, param);
                    if (exec_select(t_name, param, result, msg.payload)) { msg.cmd = CMD_SELECT; strcpy(msg.payload, result); } 
                    else msg.cmd = CMD_ERROR;
                }
                break;
            case CMD_DELETE:
                sscanf(msg.payload, "%s %d", t_name, &a.acc_no);
                if (exec_delete(t_name, a.acc_no, msg.payload)) { msg.cmd = CMD_DELETE; strcpy(msg.payload, "Account deleted."); } 
                else msg.cmd = CMD_ERROR;
                break;
        }
        write(client_socket, &msg, sizeof(msg)); 
    }

cleanup:
    log_event("Client disconnected.");
    close(client_socket);
    pthread_mutex_lock(&conn_mutex); active_connections--; pthread_mutex_unlock(&conn_mutex); 
    free(arg); pthread_exit(NULL); 
}

int main() {
    int client_fd, opt = 1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id, logger_tid;

    struct sigaction sa; sa.sa_handler = handle_shutdown; sigemptyset(&sa.sa_mask); sa.sa_flags = 0; sigaction(SIGINT, &sa, NULL);
    pthread_mutex_init(&conn_mutex, NULL);
    pthread_mutex_init(&db_mutex, NULL); 

    server_mq_id = msgget(SERVER_LOG_MQ_KEY, 0666 | IPC_CREAT);
    if (server_mq_id != -1) {
        pthread_create(&logger_tid, NULL, logger_thread_func, NULL);
        pthread_detach(logger_tid);
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY; address.sin_port = htons(PORT); 

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);
    
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║     Banking Engine Listening...      ║\n");
    printf("╚══════════════════════════════════════╝\n");

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if(client_fd < 0) continue;

        pthread_mutex_lock(&conn_mutex); active_connections++; pthread_mutex_unlock(&conn_mutex); 
        int *arg = malloc(sizeof(int)); *arg = client_fd;
        
        if (pthread_create(&thread_id, NULL, handle_client_thread, arg) < 0) {
            close(client_fd); free(arg);
        } else pthread_detach(thread_id); 
    }
    return 0;
}
