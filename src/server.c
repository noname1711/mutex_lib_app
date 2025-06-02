#include "../inc/common.h"
#include "../inc/mutex.h"
#include <signal.h>
#include <sys/select.h>

// Server socket file descriptors
static int server_fd = -1;
static int web_fd = -1;


// Close open sockets
void cleanup() {
    if (server_fd != -1) close(server_fd);
    if (web_fd != -1) close(web_fd);
    printf("Server sockets closed\n");
}


// Signal handler for graceful shutdown
void handle_signal(int sig) {
    (void)sig; // Explicitly mark as unused
    printf("\nServer shutting down...\n");
    cleanup();
    exit(EXIT_SUCCESS);
}


// Thread function to handle a single client
void* handle_client(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);  // Free the allocated memory for client socket
    
    int client_pid = -1;

    // Receive client PID
    if (recv(client_socket, &client_pid, sizeof(client_pid), 0) != sizeof(client_pid)) {
        perror("Failed to receive client PID");
        close(client_socket);
        return NULL;
    }
    
    printf("Client connected (PID: %d)\n", client_pid);
    
    ClientCommand cmd;
    char response[BUFFER_SIZE];
    
    // Initialize response buffer
    while (1) {
        // Receive command from client
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
        
        // Handle command type
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
        
        // Send response back to client
        if (send(client_socket, response, strlen(response), 0) < 0) {
            perror("send failed");
            break;
        }
    }
    
    close(client_socket);  // Close the client socket
    return NULL;
}


void handle_web_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    
    // Receive HTTP request from client
    bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }
    
    buffer[bytes_received] = '\0';
    
    // If request is GET /mutexes
    if (strstr(buffer, "GET /mutexes")) {
        char json_response[2048];
        pthread_mutex_lock(&global_mutex_lock);
        
        // Build JSON response, preparing HTTP headers
        snprintf(json_response, sizeof(json_response), 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: close\r\n\r\n"
                "{\"mutexes\":[");
        
        // Add each mutex to JSON
        for (int i = 0; i < mutex_count; i++) {
            char time_buf[20];
            if (mutexes[i].lock_time > 0) {
                strftime(time_buf, sizeof(time_buf), "%H:%M:%S", localtime(&mutexes[i].lock_time));
            } else {
                strcpy(time_buf, "null");
            }
            
            char mutex_json[256];
            
            snprintf(mutex_json, sizeof(mutex_json), 
                    "%s{\"name\":\"%s\",\"owner\":%d,\"locked\":%s,\"last_message\":\"%.50s\"}",
                    (i > 0) ? "," : "",
                    mutexes[i].name,
                    mutexes[i].owner_pid,
                    mutexes[i].is_locked ? "true" : "false",
                    mutexes[i].last_message);
            
            strncat(json_response, mutex_json, sizeof(json_response) - strlen(json_response) - 1);
        }
        
        strcat(json_response, "]}");
        pthread_mutex_unlock(&global_mutex_lock);
        
        // Send JSON response to web client
        send(client_socket, json_response, strlen(json_response), 0);
    }
    
    close(client_socket);
}

int main() {
    // Handle Ctrl+C (SIGINT) and termination (SIGTERM) signals
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Call cleanup() if the program exits
    atexit(cleanup);
    
    struct sockaddr_in address;  // Point to server address structure
    int opt = 1; // Option = true
    int addrlen = sizeof(address);
    
    // Initialize mutexes
    mutex_init();
    
    // Create main server socket (for mutex clients)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Allow reuse of the address/port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    // Set up server address and port
    address.sin_family = AF_INET;  // IPv4
    address.sin_addr.s_addr = INADDR_ANY;  // Accept connections from any IP
    address.sin_port = htons(SERVER_PORT);  // Use defined server port (8080)
    
    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Start listening for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    // Create web interface socket (main port + 1)
    if ((web_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("web socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Reuse the same address struct, just change the port (port +1)
    struct sockaddr_in web_addr = address;
    web_addr.sin_port = htons(SERVER_PORT + 1);
    
    // Bind the web socket to the new port
    if (bind(web_fd, (struct sockaddr *)&web_addr, sizeof(web_addr)) < 0) {
        perror("web bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Start listening for web clients
    if (listen(web_fd, 5) < 0) {
        perror("web listen");
        exit(EXIT_FAILURE);
    }
    
    // Print server status
    printf("Server started:\n- Mutex port: %d\n- Web port: %d\n", 
           SERVER_PORT, SERVER_PORT + 1);
    
    fd_set readfds;  // File descriptor set for select

    while (1) {

        // Clear and set the file descriptors
        FD_ZERO(&readfds);  // Clear the read file descriptor set
        FD_SET(server_fd, &readfds);  // Add server socket to the set
        FD_SET(web_fd, &readfds);  // Add web socket to the set
        
        int max_fd = (server_fd > web_fd) ? server_fd : web_fd;
        
        // Wait for activity on either socket
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            continue;
        }
        
        // If mutex client connects
        if (FD_ISSET(server_fd, &readfds)) {  // Checks if server_fd is ready for reading in the set readfds.
            int* client_socket = malloc(sizeof(int));
            *client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            
            // Create a new thread to handle the client
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, handle_client, (void*)client_socket);
            pthread_detach(thread_id);  // Auto clean up thread when done
        }
        
        // If web client connects
        if (FD_ISSET(web_fd, &readfds)) {  // Checks if web_fd is ready for reading in the set readfds.
            int web_client = accept(web_fd, NULL, NULL);

            // Handle web client directly (no thread)
            handle_web_client(web_client);
        }
    }
    
    return 0;
}