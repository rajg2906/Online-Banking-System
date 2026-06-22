#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <signal.h>
#include "common.h"

void print_prompt(char* role) {
    printf("\nminiDB(%s) > ", role); 
    fflush(stdout);
}

void print_menu() {
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────┐\n");
    printf("│                      AVAILABLE COMMANDS                      │\n");
    printf("├──────────────────────────────────────────────────────────────┤\n");
    printf("│  %-38s : %-18s │\n", "SHOW TABLES", "List all tables");
    printf("│  %-38s : %-18s │\n", "CREATE [table]", "New table (ADMIN)");
    printf("│  %-38s : %-18s │\n", "DROP [table]", "Drop table (ADMIN)");
    printf("│  %-38s : %-18s │\n", "INSERT [table] [acc_no] [name] [bal]", "Add account");
    printf("│  %-38s : %-18s │\n", "SELECT [table] *", "Retrieve all");
    printf("│  %-38s : %-18s │\n", "SELECT [table] [acc_no]", "Get account");
    printf("│  %-38s : %-18s │\n", "UPDATE [table] [acc_no] [new_bal]", "Update balance");
    printf("│  %-38s : %-18s │\n", "DELETE [table] [acc_no]", "Delete record");
    printf("│  %-38s : %-18s │\n", "logout", "End user session");
    printf("│  %-38s : %-18s │\n", "exit", "Disconnect fully");
    printf("└──────────────────────────────────────────────────────────────┘\n");
}

int get_user_input(int sock, char* buffer, int max_len) {
    fd_set readfds;
    
    while(1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds); 
        FD_SET(sock, &readfds);         

        if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0) {
            return 0; 
        }

        if (FD_ISSET(sock, &readfds)) {
            char dummy;
            if (recv(sock, &dummy, 1, MSG_PEEK | MSG_DONTWAIT) <= 0) {
                printf("\n\n[!] CRITICAL: Server died or closed the connection! Exiting immediately...\n");
                exit(1); 
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, max_len, stdin) != NULL) {
                return 1; 
            } 
	    
	    else {
                return 0; 
            }
        }
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[MAX_BUFFER];
    Msg msg;

    signal(SIGPIPE, SIG_IGN); 

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) return -1;

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }
    
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║      CONNECTED TO miniDB SERVER      ║\n");
    printf("╚══════════════════════════════════════╝\n");

    while(1) {
        char my_role[20];
        char pass[50] = "";

        while(1) {
            printf("\n┌──────────────────────────────────────┐\n");
            printf("│             SYSTEM LOGIN             │\n");
            printf("└──────────────────────────────────────┘\n");
            printf("  Roles: [admin, user, guest]\n");
            printf("  Enter Role: ");
            fflush(stdout);
            
            if(!get_user_input(sock, my_role, sizeof(my_role))) break;
            my_role[strcspn(my_role, "\n")] = 0; 

            if (strcmp(my_role, ROLE_ADMIN) != 0 && strcmp(my_role, ROLE_USER) != 0 && strcmp(my_role, ROLE_GUEST) != 0) {
                printf("  [!] Invalid role. Try again.\n");
                continue;
            }

            if (strcmp(my_role, ROLE_ADMIN) == 0 || strcmp(my_role, ROLE_USER) == 0) {
                printf("  Enter Password: ");
                fflush(stdout);
                
                get_user_input(sock, pass, sizeof(pass));
                pass[strcspn(pass, "\n")] = 0; 
            } 
	    
	    else {
                strcpy(pass, "none");
            }

            msg.cmd = CMD_LOGIN;
            snprintf(msg.payload, MAX_PAYLOAD, "%s:%s", my_role, pass);
            
            if (write(sock, &msg, sizeof(msg)) <= 0) {
                printf("\n[!] Connection to server lost. Exiting...\n");
                close(sock); exit(1);
            }

            if (read(sock, &msg, sizeof(msg)) <= 0) {
                printf("\n[!] Connection to server lost. Exiting...\n");
                close(sock); exit(1);
            }
            
            if (msg.cmd != CMD_ERROR) {
                printf("  [+] Login successful!\n");
                break; 
            } 
	    
	    else {
                printf("  [!] ERROR: %s\n", msg.payload);
            }
        }

        print_menu();

        while (1) {
            print_prompt(my_role); 
            
            if (!get_user_input(sock, buffer, MAX_BUFFER)) break;
            
            buffer[strcspn(buffer, "\n")] = 0; 

            if (strlen(buffer) == 0) continue;

            char *cmd_str = strtok(buffer, " ");
            if (!cmd_str) continue; 

            if (strcasecmp(cmd_str, "exit") == 0) {
                msg.cmd = CMD_EXIT;
                write(sock, &msg, sizeof(msg));
                printf("\nDisconnecting. Goodbye!\n");
                close(sock);
                return 0;
            } 
	    
	    else if (strcasecmp(cmd_str, "logout") == 0) {
            
		msg.cmd = CMD_LOGOUT;
                write(sock, &msg, sizeof(msg));
                read(sock, &msg, sizeof(msg));
                printf("\nSuccessfully logged out.\n");
                break; 
            } 
	    else if (strcasecmp(cmd_str, "SHOW") == 0) {
                char* t = strtok(NULL, " ");
                if(t && strcasecmp(t, "TABLES") == 0) {
                    msg.cmd = CMD_SHOW_TABLES;
                } 
		
		else { 
                    printf("  [!] Usage: SHOW TABLES\n"); continue; 
                }
            } 
	    
	    else if (strcasecmp(cmd_str, "CREATE") == 0) {
                msg.cmd = CMD_CREATE;
                char* t = strtok(NULL, " ");
                if(t) strcpy(msg.payload, t); else { printf("  [!] Usage: CREATE [table]\n"); continue; }
            } 
	    
	    else if (strcasecmp(cmd_str, "DROP") == 0) {
                msg.cmd = CMD_DROP;
                char* t = strtok(NULL, " ");
                if(t) strcpy(msg.payload, t); else { printf("  [!] Usage: DROP [table]\n"); continue; }
            } 
	    
	    else if (strcasecmp(cmd_str, "SELECT") == 0) {
                msg.cmd = CMD_SELECT;
                char* t = strtok(NULL, " ");
                char* param = strtok(NULL, " ");
                if(t && param) snprintf(msg.payload, MAX_PAYLOAD, "%s %s", t, param);
                else { printf("  [!] Usage: SELECT [table] [* or acc_no]\n"); continue; }
            } 
	    
	    else if (strcasecmp(cmd_str, "INSERT") == 0) {
                msg.cmd = CMD_INSERT;
                char* t = strtok(NULL, " ");
                char* acc = strtok(NULL, " ");
                char* name = strtok(NULL, " ");
                char* bal = strtok(NULL, " ");
                if (t && acc && name && bal) snprintf(msg.payload, MAX_PAYLOAD, "%s %s %s %s", t, acc, name, bal);
                else { printf("  [!] Usage: INSERT [table] [acc_no] [name] [balance]\n"); continue; }
            } 
	    
	    else if (strcasecmp(cmd_str, "UPDATE") == 0) {
                msg.cmd = CMD_UPDATE;
                char* t = strtok(NULL, " ");
                char* acc = strtok(NULL, " ");
                char* bal = strtok(NULL, " ");
                if (t && acc && bal) snprintf(msg.payload, MAX_PAYLOAD, "%s %s %s", t, acc, bal);
                else { printf("  [!] Usage: UPDATE [table] [acc_no] [new_balance]\n"); continue; }
            } 
	    
	    else if (strcasecmp(cmd_str, "DELETE") == 0) {
                msg.cmd = CMD_DELETE;
                char* t = strtok(NULL, " ");
                char* acc = strtok(NULL, " ");
                if(t && acc) snprintf(msg.payload, MAX_PAYLOAD, "%s %s", t, acc);
                else { printf("  [!] Usage: DELETE [table] [acc_no]\n"); continue; }
            } 
	    
	    else {
                printf("  [!] ERROR: Unknown command.\n"); continue;
            }

            if (write(sock, &msg, sizeof(msg)) <= 0) {
                printf("\n[!] Connection to server lost. Exiting...\n");
                close(sock); exit(1);
            }
            
	    if (read(sock, &msg, sizeof(msg)) <= 0) {
                printf("\n[!] Connection to server lost. Exiting...\n");
                close(sock); exit(1);
            }

            if (msg.cmd == CMD_ERROR) {
                printf("\n┌──────────────────────── ERROR ────────────────────────┐\n");
                printf("  %-53s \n", msg.payload);
                printf("└───────────────────────────────────────────────────────┘\n");
            } 
	    
	    else {
                printf("\n┌─────────────────────── SUCCESS ───────────────────────┐\n");
                if(msg.payload[0] != '\0') printf("  %s", msg.payload);
                if (msg.payload[strlen(msg.payload)-1] != '\n') printf("\n");
                printf("└───────────────────────────────────────────────────────┘\n");
            }
        }
    }
    close(sock); 
    return 0;
}
