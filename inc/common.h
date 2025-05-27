#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#define MAX_MUTEXES 20
#define MAX_CLIENTS 50
#define BUFFER_SIZE 2048
#define SERVER_PORT 8080
#define MAX_MUTEX_NAME 64
#define MAX_MSG_SIZE 1024

typedef enum {
    CMD_HELP,
    CMD_CREATE,
    CMD_LOCK,
    CMD_UNLOCK,
    CMD_LIST,
    CMD_DELETE,
    CMD_SEND,
    CMD_EXIT,
    CMD_INVALID
} CommandType;

typedef struct {
    char name[MAX_MUTEX_NAME];
    int owner_pid;
    bool is_locked;
    time_t lock_time;
} Mutex;

typedef struct {
    CommandType type;
    char mutex_name[MAX_MUTEX_NAME];
    char message[MAX_MSG_SIZE];
    int client_pid;
} ClientCommand;

#endif 
