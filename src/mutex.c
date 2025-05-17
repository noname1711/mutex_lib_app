#include "../inc/mutex.h"
#include "../inc/common.h"

static Mutex mutexes[MAX_MUTEXES];
static int mutex_count = 0;
static pthread_mutex_t global_mutex_lock = PTHREAD_MUTEX_INITIALIZER;

void mutex_init() {
    pthread_mutex_lock(&global_mutex_lock);
    mutex_count = 0;
    for (int i = 0; i < MAX_MUTEXES; i++) {
        mutexes[i].name[0] = '\0';
        mutexes[i].owner_pid = -1;
        mutexes[i].is_locked = false;
        mutexes[i].lock_time = 0;
    }
    pthread_mutex_unlock(&global_mutex_lock);
}

int mutex_create(const char* name, int client_pid) {
    if (strlen(name) == 0) return -1;
    
    pthread_mutex_lock(&global_mutex_lock);
    
    if (mutex_count >= MAX_MUTEXES) {
        pthread_mutex_unlock(&global_mutex_lock);
        return -2;
    }

    for (int i = 0; i < mutex_count; i++) {
        if (strcmp(mutexes[i].name, name) == 0) {
            pthread_mutex_unlock(&global_mutex_lock);
            return -3;
        }
    }

    strncpy(mutexes[mutex_count].name, name, MAX_MUTEX_NAME - 1);
    mutexes[mutex_count].owner_pid = client_pid;
    mutexes[mutex_count].is_locked = false;
    mutexes[mutex_count].lock_time = 0;
    mutex_count++;
    
    pthread_mutex_unlock(&global_mutex_lock);
    return 0;
}

int mutex_lock(const char* name, int client_pid) {
    pthread_mutex_lock(&global_mutex_lock);
    
    for (int i = 0; i < mutex_count; i++) {
        if (strcmp(mutexes[i].name, name) == 0) {
            if (mutexes[i].is_locked) {
                if (mutexes[i].owner_pid == client_pid) {
                    pthread_mutex_unlock(&global_mutex_lock);
                    return 0; // Already locked by this client
                }
                pthread_mutex_unlock(&global_mutex_lock);
                return -1; // Locked by another client
            }
            
            mutexes[i].is_locked = true;
            mutexes[i].owner_pid = client_pid;
            mutexes[i].lock_time = time(NULL);
            pthread_mutex_unlock(&global_mutex_lock);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&global_mutex_lock);
    return -2; // Mutex not found
}

int mutex_unlock(const char* name, int client_pid) {
    pthread_mutex_lock(&global_mutex_lock);
    
    for (int i = 0; i < mutex_count; i++) {
        if (strcmp(mutexes[i].name, name) == 0) {
            if (!mutexes[i].is_locked) {
                pthread_mutex_unlock(&global_mutex_lock);
                return -1; // Already unlocked
            }
            if (mutexes[i].owner_pid != client_pid) {
                pthread_mutex_unlock(&global_mutex_lock);
                return -2; // Not owned by this client
            }
            
            mutexes[i].is_locked = false;
            mutexes[i].lock_time = 0;
            pthread_mutex_unlock(&global_mutex_lock);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&global_mutex_lock);
    return -3; // Mutex not found
}

int mutex_delete(const char* name, int client_pid) {
    pthread_mutex_lock(&global_mutex_lock);
    
    for (int i = 0; i < mutex_count; i++) {
        if (strcmp(mutexes[i].name, name) == 0) {
            if (mutexes[i].is_locked && mutexes[i].owner_pid != client_pid) {
                pthread_mutex_unlock(&global_mutex_lock);
                return -1; // Locked by another client
            }
            
            // Shift remaining mutexes
            for (int j = i; j < mutex_count - 1; j++) {
                mutexes[j] = mutexes[j + 1];
            }
            mutex_count--;
            pthread_mutex_unlock(&global_mutex_lock);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&global_mutex_lock);
    return -2; // Mutex not found
}

void mutex_list(char* buffer, size_t buf_size) {
    pthread_mutex_lock(&global_mutex_lock);
    
    char header[256];
    snprintf(header, sizeof(header), 
             "Mutex List (Total: %d)\n"
             "%-20s %-10s %-10s %-20s\n"
             "--------------------------------------------------\n",
             mutex_count, "Name", "Owner PID", "Locked", "Lock Time");
    
    size_t offset = strlen(header);
    strncpy(buffer, header, buf_size - 1);
    
    for (int i = 0; i < mutex_count && offset < buf_size - 200; i++) {
        char line[200];
        char time_buf[20];
        
        if (mutexes[i].lock_time > 0) {
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", 
                    localtime(&mutexes[i].lock_time));
        } else {
            strcpy(time_buf, "N/A");
        }
        
        snprintf(line, sizeof(line), "%-20s %-10d %-10s %-20s\n",
                mutexes[i].name,
                mutexes[i].owner_pid,
                mutexes[i].is_locked ? "Yes" : "No",
                time_buf);
        
        strncat(buffer + offset, line, buf_size - offset - 1);
        offset += strlen(line);
    }
    
    pthread_mutex_unlock(&global_mutex_lock);
}

int mutex_send(const char* name, int client_pid, const char* message, 
               char* response, size_t resp_size, char* welcome_msg, size_t welcome_size) {
    pthread_mutex_lock(&global_mutex_lock);
    
    for (int i = 0; i < mutex_count; i++) {
        if (strcmp(mutexes[i].name, name) == 0) {
            if (!mutexes[i].is_locked || mutexes[i].owner_pid != client_pid) {
                pthread_mutex_unlock(&global_mutex_lock);
                snprintf(response, resp_size, "Cannot send: you don't own mutex '%.20s'", name);
                return -1;
            }
            
            // Safe message formatting with proper size_t comparison
            int msg_len = snprintf(response, resp_size, 
                                 "Message received via mutex '%.20s' from PID %d: %.200s",
                                 name, client_pid, message);
            if (msg_len >= 0 && (size_t)msg_len >= resp_size) {
                response[resp_size - 1] = '\0';
            }
            
            // Create welcome message with proper size_t comparison
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            char time_str[20];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
            
            int welcome_len = snprintf(welcome_msg, welcome_size,
                                     "Welcome client PID %d! Sent message successfully at %s",
                                     client_pid, time_str);
            if (welcome_len >= 0 && (size_t)welcome_len >= welcome_size) {
                welcome_msg[welcome_size - 1] = '\0';
            }
            
            pthread_mutex_unlock(&global_mutex_lock);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&global_mutex_lock);
    snprintf(response, resp_size, "Mutex '%.20s' not found", name);
    return -2;
}

bool mutex_has_permission(const char* name, int client_pid) {
    pthread_mutex_lock(&global_mutex_lock);
    
    for (int i = 0; i < mutex_count; i++) {
        if (strcmp(mutexes[i].name, name) == 0) {
            bool result = (!mutexes[i].is_locked || mutexes[i].owner_pid == client_pid);
            pthread_mutex_unlock(&global_mutex_lock);
            return result;
        }
    }
    
    pthread_mutex_unlock(&global_mutex_lock);
    return false;
}

CommandType parse_command(const char* cmd) {
    if (strcasecmp(cmd, "help") == 0) return CMD_HELP;
    if (strcasecmp(cmd, "create") == 0) return CMD_CREATE;
    if (strcasecmp(cmd, "lock") == 0) return CMD_LOCK;
    if (strcasecmp(cmd, "unlock") == 0) return CMD_UNLOCK;
    if (strcasecmp(cmd, "list") == 0) return CMD_LIST;
    if (strcasecmp(cmd, "delete") == 0) return CMD_DELETE;
    if (strcasecmp(cmd, "send") == 0) return CMD_SEND;
    if (strcasecmp(cmd, "exit") == 0) return CMD_EXIT;
    return CMD_INVALID;
}

const char* command_to_string(CommandType cmd) {
    switch (cmd) {
        case CMD_HELP: return "HELP";
        case CMD_CREATE: return "CREATE";
        case CMD_LOCK: return "LOCK";
        case CMD_UNLOCK: return "UNLOCK";
        case CMD_LIST: return "LIST";
        case CMD_DELETE: return "DELETE";
        case CMD_SEND: return "SEND";
        case CMD_EXIT: return "EXIT";
        default: return "INVALID";
    }
}

void print_help() {
    printf("\nAvailable commands:\n");
    printf("help                 - Show this help message\n");
    printf("create <mutex_name>  - Create a new mutex\n");
    printf("lock <mutex_name>    - Lock a mutex (gain ownership)\n");
    printf("unlock <mutex_name>  - Unlock a mutex (release ownership)\n");
    printf("list                 - List all mutexes and their status\n");
    printf("delete <mutex_name>  - Delete a mutex\n");
    printf("send <mutex> <msg>   - Send message (requires ownership)\n");
    printf("exit                 - Exit the client\n\n");
}