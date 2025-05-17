#ifndef MUTEX_H
#define MUTEX_H

#include "common.h"

// Server-side API
void mutex_init();
int mutex_create(const char* name, int client_pid);
int mutex_lock(const char* name, int client_pid);
int mutex_unlock(const char* name, int client_pid);
int mutex_delete(const char* name, int client_pid);
void mutex_list(char* buffer, size_t buf_size);
int mutex_send(const char* name, int client_pid, const char* message, 
               char* response, size_t resp_size, char* welcome_msg, size_t welcome_size);
bool mutex_has_permission(const char* name, int client_pid);

// Client-side helpers
CommandType parse_command(const char* cmd);
void print_help();
const char* command_to_string(CommandType cmd);

#endif 