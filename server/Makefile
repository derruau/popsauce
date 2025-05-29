CC = gcc
CFLAGS = -I./include -I../ -Wall -Wextra -g
TARGET = popsauce-server

SRC_DIR = ./src
COMMON_SRC = ../common.c
SQLITE_DIR = ./lib/sqlite
OBJ_DIR = ./build

# Collect source files
SRCS := $(wildcard $(SRC_DIR)/*.c)
SRCS += $(COMMON_SRC)

# Generate object file paths
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.c))
OBJS += $(OBJ_DIR)/common.o

# SQLite source and object
SQLITE_SRC = $(SQLITE_DIR)/sqlite3.c
SQLITE_OBJ = $(SQLITE_DIR)/sqlite3.o

# Special object
QUESTIONS_OBJ = $(OBJ_DIR)/questions.o

.PHONY: all clean

all: $(SQLITE_OBJ) $(TARGET)

$(TARGET): $(OBJS) $(SQLITE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

# Compile questions.c with awareness of sqlite (no special flags needed now, sqlite3.o is compiled separately)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile common.c
$(OBJ_DIR)/common.o: ../common.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile sqlite3.o if not already done
$(SQLITE_OBJ): $(SQLITE_SRC)
	$(CC) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(SQLITE_OBJ)