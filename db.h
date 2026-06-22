#ifndef DB_H
#define DB_H

#include "common.h"
#include <pthread.h>

extern pthread_mutex_t db_mutex;

void get_db_file_path(char* table_name, char* path_out);
int set_posix_lock(int fd, short type);
int exec_show_tables(char* result_out, char* error_payload);
int exec_create(char* table_name, char* error_payload);
int exec_drop(char* table_name, char* error_payload);
int exec_insert(char* table_name, Account *new_acc, char* error_payload);
int exec_select(char* table_name, char* param, char* result_out, char* error_payload);
int exec_update(char* table_name, int acc_no, float new_bal, char* error_payload);
int exec_delete(char* table_name, int target_no, char* error_payload);

#endif
