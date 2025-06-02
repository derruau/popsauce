CC = gcc
CFLAGS = -I./include -I../ -Wall -Wextra -g
LDFLAGS = -lncurses -lform -lmenu -lm
TARGET = popsauce-client

SRC_DIR = ./src
COMMON_SRC = ../common.c
LIBSIXEL_PATH = ./lib/libsixel.a
OBJ_DIR = ./build

# Collect source files
SRCS := $(wildcard $(SRC_DIR)/*.c)
SRCS += $(COMMON_SRC)

# Generate object file paths
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(wildcard $(SRC_DIR)/*.c))
OBJS += $(OBJ_DIR)/common.o

.PHONY: all clean

# Default target to build the executable
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBSIXEL_PATH) $(LDFLAGS) -o $@

# Compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile common.c
$(OBJ_DIR)/common.o: $(COMMON_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure the object directory exists
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean up build artifacts
clean:
	rm -rf $(OBJ_DIR) $(TARGET)