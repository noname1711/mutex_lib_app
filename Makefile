.PHONY: all server client lib clean directories

CC = gcc
CFLAGS = -Wall -Wextra -I./inc -g
LDFLAGS = -lpthread
AR = ar
ARFLAGS = rcs

SRC_DIR = ./src
INC_DIR = ./inc
OBJ_DIR = ./obj
LIB_DIR = ./lib
BIN_DIR = ./bin

SERVER_SRC = $(SRC_DIR)/server.c
CLIENT_SRC = $(SRC_DIR)/client.c
MUTEX_SRC = $(SRC_DIR)/mutex.c

SERVER_OBJ = $(OBJ_DIR)/server.o
CLIENT_OBJ = $(OBJ_DIR)/client.o
MUTEX_OBJ = $(OBJ_DIR)/mutex.o

LIB_NAME = $(LIB_DIR)/libmutex.a

SERVER_TARGET = $(BIN_DIR)/server
CLIENT_TARGET = $(BIN_DIR)/client

all: directories lib server client

directories:
	mkdir -p $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR)

lib: $(MUTEX_OBJ)
	$(AR) $(ARFLAGS) $(LIB_NAME) $^

server: $(SERVER_OBJ) $(LIB_NAME)
	$(CC) $(CFLAGS) $^ -o $(SERVER_TARGET) $(LDFLAGS)

client: $(CLIENT_OBJ) $(LIB_NAME)
	$(CC) $(CFLAGS) $^ -o $(CLIENT_TARGET) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR)