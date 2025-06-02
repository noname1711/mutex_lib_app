#include "../inc/common.h"
#include "../inc/mutex.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int main() {
    int sock = 0;   // Socket file descriptor
    struct sockaddr_in serv_addr; // Server address structure
    char input[BUFFER_SIZE];  // Buffer for user input
    int client_pid = getpid();  // Get current process ID
    
    printf("Client started (PID: %d)\n", client_pid);
    print_help();
    
    // Create socket using IPv4 stream
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET; // IPv4
    serv_addr.sin_port = htons(SERVER_PORT);  // Server port in network byte order
    
    // Convert IP address string to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        return -1;
    }
    
    // Connect client to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }
    
    // Send PID to server first
    if (send(sock, &client_pid, sizeof(client_pid), 0) != sizeof(client_pid)) {
        perror("Failed to send PID");
        close(sock);
        return -1;
    }
    
    while (1) {
        printf("[PID:%d]> ", client_pid);   // Prompt for input
        fgets(input, sizeof(input), stdin);  // Read user input
        input[strcspn(input, "\n")] = '\0'; // Remove newline
        
        if (strlen(input) == 0) continue;  // Skip empty input
        
        ClientCommand cmd;
        cmd.client_pid = client_pid;   // Set client PID in command structure
        memset(cmd.mutex_name, 0, sizeof(cmd.mutex_name));  // Clear mutex name
        memset(cmd.message, 0, sizeof(cmd.message));   // Clear message
        
        // Parse first word as command
        char *token = strtok(input, " ");
        cmd.type = parse_command(token);
        
        if (cmd.type == CMD_INVALID) {
            printf("Invalid command. Type 'help' for available commands.\n");
            continue;
        }
        
        if (cmd.type == CMD_HELP) {
            print_help();
            continue;
        }
        
        if (cmd.type == CMD_EXIT) {
            cmd.type = CMD_EXIT;
            send(sock, &cmd, sizeof(cmd), 0);
            break;
        }
        
        // Handle commands with mutex name
        if (cmd.type == CMD_CREATE || cmd.type == CMD_LOCK || 
            cmd.type == CMD_UNLOCK || cmd.type == CMD_DELETE || cmd.type == CMD_SEND) {
            
            token = strtok(NULL, " ");   // Get mutex name
            if (token == NULL) {
                printf("Error: Mutex name required for command '%s'\n", command_to_string(cmd.type));
                continue;
            }

            strncpy(cmd.mutex_name, token, MAX_MUTEX_NAME - 1);
            
            // For SEND command, get the message
            if (cmd.type == CMD_SEND) {
                token = strtok(NULL, "");
                if (token == NULL) {
                    printf("Error: Message required for 'send' command\n");
                    continue;
                }
                
                // Copy message safely with limit
                size_t msg_len = strlen(token);
                size_t copy_len = msg_len < MAX_MSG_SIZE - 1 ? msg_len : MAX_MSG_SIZE - 1;
                strncpy(cmd.message, token, copy_len);
                cmd.message[copy_len] = '\0';
                
                // Preview message sent
                printf("[PID:%d] Sending via '%.20s': %.50s%s\n", 
                    client_pid, 
                    cmd.mutex_name, 
                    cmd.message, 
                    msg_len > 50 ? "..." : "");
            }
        }
                
        // Send command to server
        if (send(sock, &cmd, sizeof(cmd), 0) < 0) {
            perror("send failed");
                break;
        }
        
        // Receive response from server
        char response[BUFFER_SIZE] = {0};
        ssize_t valread = recv(sock, response, sizeof(response) - 1, 0);
        if (valread <= 0) {
            if (valread == 0) {
                printf("Server disconnected\n");
            } else {
                perror("recv failed");
            }
            break;
        }
        
        printf("Server: %s\n", response);  // Print server response
    }
    
    close(sock);  // Close the socket
    printf("Client (PID: %d) exiting...\n", client_pid);
    return 0;
}