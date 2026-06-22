#ifndef COMMON_H
#define COMMON_H

#define PORT 8080 
#define MAX_BUFFER 256
#define MAX_PAYLOAD 1024 

#define CMD_LOGIN       1
#define CMD_CREATE      2
#define CMD_SELECT      3
#define CMD_INSERT      4
#define CMD_UPDATE      5
#define CMD_DELETE      6
#define CMD_EXIT        7
#define CMD_LOGOUT      8
#define CMD_SHOW_TABLES 9 
#define CMD_DROP        10
#define CMD_ERROR       0

#define ROLE_ADMIN "admin"
#define ROLE_USER  "user"
#define ROLE_GUEST "guest"

typedef struct {
    int acc_no;        
    char name[50];
    float balance;
} Account;

typedef struct {
    int cmd;
    char payload[MAX_PAYLOAD];
} Msg;

#define SERVER_LOG_MQ_KEY 0x1234
typedef struct {
    long mtype;
    char mtext[MAX_BUFFER];
} LogMsg;

#endif
