.PHONY: all server client lib clean directories web

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

WEB_SRC = $(SRC_DIR)/web_server.c
WEB_OBJ = $(OBJ_DIR)/web_server.o
WEB_TARGET = $(BIN_DIR)/web_server

all: directories lib server client web_server

directories:
	mkdir -p $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR)

lib: $(MUTEX_OBJ)
	$(AR) $(ARFLAGS) $(LIB_NAME) $^

server: $(SERVER_OBJ) $(LIB_NAME)
	$(CC) $(CFLAGS) $^ -o $(SERVER_TARGET) $(LDFLAGS)

client: $(CLIENT_OBJ) $(LIB_NAME)
	$(CC) $(CFLAGS) $^ -o $(CLIENT_TARGET) $(LDFLAGS)

web_server: $(WEB_OBJ) $(LIB_NAME)
	$(CC) $(CFLAGS) $^ -o $(WEB_TARGET) $(LDFLAGS) -lmicrohttpd 

web:
	@echo "Starting web server on http://localhost:8000"
	@python3 -m http.server 8000 --directory web/

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR)