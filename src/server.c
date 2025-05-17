#include "../inc/common.h"
#include "../inc/mutex.h"
#include <signal.h>

static int server_fd = -1;

void cleanup() {
    if (server_fd != -1) {
        close(server_fd);
        printf("Server socket closed\n");
    }
}

void handle_signal(int sig) {
    (void)sig; // Explicitly mark as unused
    printf("\nServer shutting down...\n");
    cleanup();
    exit(EXIT_SUCCESS);
}

void* handle_client(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);
    
    int client_pid = -1;
    if (recv(client_socket, &client_pid, sizeof(client_pid), 0) != sizeof(client_pid)) {
        perror("Failed to receive client PID");
        close(client_socket);
        return NULL;
    }
    
    printf("Client connected (PID: %d)\n", client_pid);
    
    ClientCommand cmd;
    char response[BUFFER_SIZE];
    
    while (1) {
        ssize_t bytes_received = recv(client_socket, &cmd, sizeof(cmd), 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Client (PID: %d) disconnected\n", client_pid);
            } else {
                perror("recv failed");
            }
            break;
        }
        
        response[0] = '\0';
        int status = 0;
        
        switch (cmd.type) {
            case CMD_HELP:
                snprintf(response, sizeof(response), 
                        "Available commands: create, lock, unlock, list, delete, send");
                break;
                
            case CMD_CREATE:
                status = mutex_create(cmd.mutex_name, client_pid);
                if (status == 0) {
                    snprintf(response, sizeof(response), "Mutex '%s' created", cmd.mutex_name);
                } else if (status == -2) {
                    strcpy(response, "Cannot create mutex: maximum limit reached");
                } else {
                    snprintf(response, sizeof(response), "Mutex '%s' already exists", cmd.mutex_name);
                }
                break;
                
            case CMD_LOCK:
                status = mutex_lock(cmd.mutex_name, client_pid);
                if (status == 0) {
                    snprintf(response, sizeof(response), "Mutex '%s' locked", cmd.mutex_name);
                } else if (status == -1) {
                    snprintf(response, sizeof(response), 
                            "Mutex '%s' already locked by another client", cmd.mutex_name);
                } else {
                    snprintf(response, sizeof(response), "Mutex '%s' not found", cmd.mutex_name);
                }
                break;
                
            case CMD_UNLOCK:
                status = mutex_unlock(cmd.mutex_name, client_pid);
                if (status == 0) {
                    snprintf(response, sizeof(response), "Mutex '%s' unlocked", cmd.mutex_name);
                } else if (status == -1) {
                    snprintf(response, sizeof(response), "Mutex '%s' already unlocked", cmd.mutex_name);
                } else if (status == -2) {
                    snprintf(response, sizeof(response), 
                            "Cannot unlock: you don't own mutex '%s'", cmd.mutex_name);
                } else {
                    snprintf(response, sizeof(response), "Mutex '%s' not found", cmd.mutex_name);
                }
                break;
                
            case CMD_LIST:
                mutex_list(response, sizeof(response));
                break;
                
            case CMD_DELETE:
                status = mutex_delete(cmd.mutex_name, client_pid);
                if (status == 0) {
                    snprintf(response, sizeof(response), "Mutex '%s' deleted", cmd.mutex_name);
                } else if (status == -1) {
                    snprintf(response, sizeof(response), 
                            "Cannot delete: mutex '%s' is locked by another client", cmd.mutex_name);
                } else {
                    snprintf(response, sizeof(response), "Mutex '%s' not found", cmd.mutex_name);
                }
                break;
                
            case CMD_SEND: {
                char detailed_response[BUFFER_SIZE];
                char welcome_message[BUFFER_SIZE];
                
                status = mutex_send(cmd.mutex_name, cmd.client_pid, cmd.message, 
                                detailed_response, sizeof(detailed_response),
                                welcome_message, sizeof(welcome_message));
                
                // Display on server console (safe truncated output)
                printf("%.200s\n", detailed_response);
                printf("Sending message to PID %d\n", cmd.client_pid);
                
                if (status == 0) {
                    // Calculate space needed for each part
                    size_t available = sizeof(response);
                    size_t used = 0;
                    
                    // Format SUCCESS header
                    used = snprintf(response, available, "SUCCESS:\n");
                    
                    // Add detailed response if space available
                    if (used < available) {
                        int needed = snprintf(NULL, 0, "%.*s\n", 
                                            (int)(available - used - 50), detailed_response);
                        used += snprintf(response + used, available - used, "%.*s\n",
                                    needed < (int)(available - used) ? needed : (int)(available - used - 1),
                                    detailed_response);
                    }
                    
                    // Add SERVER REPLY header if space available
                    if (used < available) {
                        used += snprintf(response + used, available - used, "SERVER REPLY:\n");
                    }
                    
                    // Add welcome message if space available
                    if (used < available) {
                        snprintf(response + used, available - used, "%.*s",
                            (int)(available - used - 1), welcome_message);
                    }
                } else {
                    // If failed, just send the error message
                    strncpy(response, detailed_response, sizeof(response) - 1);
                    response[sizeof(response) - 1] = '\0';
                }
                break;
            }
                
            case CMD_EXIT:
                printf("Client (PID: %d) requested exit\n", client_pid);
                strcpy(response, "Goodbye!");
                send(client_socket, response, strlen(response), 0);
                close(client_socket);
                return NULL;
                
            default:
                strcpy(response, "Invalid command. Type 'help' for available commands.");
                break;
        }
        
        if (send(client_socket, response, strlen(response), 0) < 0) {
            perror("send failed");
            break;
        }
    }
    
    close(client_socket);
    return NULL;
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(cleanup);
    
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    mutex_init();
    
    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);
    
    // Bind socket to port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Start listening
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started on port %d. Waiting for connections...\n", SERVER_PORT);
    
    while (1) {
        int* client_socket = malloc(sizeof(int));
        if ((*client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            free(client_socket);
            continue;
        }
        
        printf("New connection from %s\n", inet_ntoa(address.sin_addr));
        
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)client_socket) != 0) {
            perror("pthread_create");
            close(*client_socket);
            free(client_socket);
        }
        
        pthread_detach(thread_id);
    }
    
    return 0;
}