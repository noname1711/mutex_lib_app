.PHONY: all server client lib clean directories web

# Compiler and flags
CC = gcc   # Command used to compile the source files
CFLAGS = -Wall -Wextra -I./inc -g  # Enable all warnings, include directory, and debug info
LDFLAGS = -lpthread  # Link with pthread library
AR = ar # Command used to create the static library
ARFLAGS = rcs   # Archive flags: replace, create, index

# Directory structure
SRC_DIR = ./src
INC_DIR = ./inc
OBJ_DIR = ./obj
LIB_DIR = ./lib
BIN_DIR = ./bin

# Source files
SERVER_SRC = $(SRC_DIR)/server.c
CLIENT_SRC = $(SRC_DIR)/client.c
MUTEX_SRC = $(SRC_DIR)/mutex.c

# Object files 
SERVER_OBJ = $(OBJ_DIR)/server.o
CLIENT_OBJ = $(OBJ_DIR)/client.o
MUTEX_OBJ = $(OBJ_DIR)/mutex.o

# Static library
LIB_NAME = $(LIB_DIR)/libmutex.a

# Final binaries
SERVER_TARGET = $(BIN_DIR)/server
CLIENT_TARGET = $(BIN_DIR)/client

# Web server
WEB_SRC = $(SRC_DIR)/web_server.c
WEB_TARGET = $(BIN_DIR)/web_server

# Default target: build everything
all: directories lib server client 

# Create necessary directories if they do not exist
directories:
	mkdir -p $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR)

# Build the static library
lib: $(MUTEX_OBJ)
	$(AR) $(ARFLAGS) $(LIB_NAME) $^

# Build the server and client
server: $(SERVER_OBJ) $(LIB_NAME)
	$(CC) $(CFLAGS) $^ -o $(SERVER_TARGET) $(LDFLAGS)

client: $(CLIENT_OBJ) $(LIB_NAME)
	$(CC) $(CFLAGS) $^ -o $(CLIENT_TARGET) $(LDFLAGS)

# Build the web server
web:
	@echo "Starting web server on http://localhost:8000"
	@python3 -m http.server 8000 --directory web/

# Compile all .c file from SRC_DIR into .o in OBJ_DIR
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR)