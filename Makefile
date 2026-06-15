# Unified Makefile for Distributed File System
# OSN Course Project - Builds Name Server, Storage Server, and Client

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -g
CFLAGS_SS = -Wall -Wextra -Wno-format-truncation -g -pthread

# Directories
COMMON_DIR = common
NM_DIR = name_server
SS_DIR = storage_server
CLIENT_DIR = client
BIN_DIR = bin

# Common files
COMMON_SRC = $(COMMON_DIR)/network_utils.c
COMMON_OBJ = $(COMMON_DIR)/network_utils.o

# Name Server files
NM_SRC = $(NM_DIR)/name_server.c $(NM_DIR)/ss_manager.c $(NM_DIR)/client_handler.c
NM_OBJ = $(NM_SRC:.c=.o)
NM_TARGET = $(NM_DIR)/name_server

# Storage Server files
SS_SRC = $(SS_DIR)/storage_server.c $(SS_DIR)/file_handler_ll.c $(SS_DIR)/file_write_ll.c \
         $(SS_DIR)/ss_nm_comm.c $(SS_DIR)/ss_client_comm.c
SS_OBJ = $(SS_SRC:.c=.o)
SS_TARGET = $(SS_DIR)/storage_server

# Client files
CLIENT_SRC = $(CLIENT_DIR)/client.c $(CLIENT_DIR)/client_nm_comm.c $(CLIENT_DIR)/client_ss_comm.c \
             $(CLIENT_DIR)/command_parser.c
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
CLIENT_TARGET = $(CLIENT_DIR)/client

# Default target - build everything
.PHONY: all
all: banner $(NM_TARGET) $(SS_TARGET) $(CLIENT_TARGET) success

# Banner
.PHONY: banner
banner:
	@echo "========================================="
	@echo " Building Distributed File System"
	@echo "========================================="

# Success message
.PHONY: success
success:
	@echo ""
	@echo "========================================="
	@echo " ✓ Build Complete!"
	@echo "========================================="
	@echo " Name Server:    $(NM_TARGET)"
	@echo " Storage Server: $(SS_TARGET)"
	@echo " Client:         $(CLIENT_TARGET)"
	@echo "========================================="
	@echo ""

# Build Name Server
$(NM_TARGET): $(NM_OBJ) $(COMMON_OBJ)
	@echo "=== Linking Name Server ==="
	$(CC) $(CFLAGS) -o $@ $^
	@echo "✓ Name Server built successfully"

# Build Storage Server
$(SS_TARGET): $(SS_OBJ) $(COMMON_OBJ)
	@echo "=== Linking Storage Server ==="
	$(CC) $(CFLAGS_SS) -o $@ $^
	@echo "✓ Storage Server built successfully"

# Build Client
$(CLIENT_TARGET): $(CLIENT_OBJ) $(COMMON_OBJ)
	@echo "=== Linking Client ==="
	$(CC) $(CFLAGS) -o $@ $^
	@echo "✓ Client built successfully"

# Compile Name Server source files
$(NM_DIR)/%.o: $(NM_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile Storage Server source files
$(SS_DIR)/%.o: $(SS_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS_SS) -I$(COMMON_DIR) -c $< -o $@

# Compile Client source files
$(CLIENT_DIR)/%.o: $(CLIENT_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile Common source files
$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c
	@echo "Compiling common: $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Individual build targets
.PHONY: nameserver nm
nameserver nm: $(NM_TARGET)
	@echo "✓ Name Server ready"

.PHONY: storageserver ss
storageserver ss: $(SS_TARGET)
	@echo "✓ Storage Server ready"

.PHONY: client
client: $(CLIENT_TARGET)
	@echo "✓ Client ready"

# Clean all build artifacts
.PHONY: clean
clean:
	@echo "Cleaning all build artifacts..."
	@rm -f $(NM_OBJ) $(NM_TARGET)
	@rm -f $(SS_OBJ) $(SS_TARGET)
	@rm -f $(CLIENT_OBJ) $(CLIENT_TARGET)
	@rm -f $(COMMON_OBJ)
	@rm -f $(NM_DIR)/name_server.log
	@rm -rf $(SS_DIR)/storage_data*
	@echo "✓ Clean complete"

# Clean only Name Server
.PHONY: clean-nm
clean-nm:
	@echo "Cleaning Name Server..."
	@rm -f $(NM_OBJ) $(NM_TARGET) $(NM_DIR)/name_server.log
	@echo "✓ Name Server cleaned"

# Clean only Storage Server
.PHONY: clean-ss
clean-ss:
	@echo "Cleaning Storage Server..."
	@rm -f $(SS_OBJ) $(SS_TARGET)
	@rm -rf $(SS_DIR)/storage_data*
	@echo "✓ Storage Server cleaned"

# Clean only Client
.PHONY: clean-client
clean-client:
	@echo "Cleaning Client..."
	@rm -f $(CLIENT_OBJ) $(CLIENT_TARGET)
	@echo "✓ Client cleaned"

# Install - copy binaries to bin directory
.PHONY: install
install: all
	@echo "Installing binaries to $(BIN_DIR)..."
	@mkdir -p $(BIN_DIR)
	@cp $(NM_TARGET) $(BIN_DIR)/
	@cp $(SS_TARGET) $(BIN_DIR)/
	@cp $(CLIENT_TARGET) $(BIN_DIR)/
	@echo "✓ Installation complete"
	@echo "  Binaries available in: $(BIN_DIR)/"

# Run Name Server (for testing)
.PHONY: run-nm
run-nm: $(NM_TARGET)
	@echo "Starting Name Server..."
	@cd $(NM_DIR) && ./name_server

# Run Storage Server (for testing) - SS1
.PHONY: run-ss1
run-ss1: $(SS_TARGET)
	@echo "Starting Storage Server 1..."
	@mkdir -p $(SS_DIR)/storage_data1
	@cd $(SS_DIR) && ./storage_server 1 127.0.0.1 9001 9002 ./storage_data1

# Run Storage Server 2
.PHONY: run-ss2
run-ss2: $(SS_TARGET)
	@echo "Starting Storage Server 2..."
	@mkdir -p $(SS_DIR)/storage_data2
	@cd $(SS_DIR) && ./storage_server 2 127.0.0.1 9003 9004 ./storage_data2

# Run Client (for testing)
.PHONY: run-client
run-client: $(CLIENT_TARGET)
	@echo "Starting Client..."
	@cd $(CLIENT_DIR) && ./client 127.0.0.1

# Help target
.PHONY: help
help:
	@echo "Distributed File System - Makefile Help"
	@echo "========================================"
	@echo ""
	@echo "Build Targets:"
	@echo "  make              - Build all components (default)"
	@echo "  make all          - Build all components"
	@echo "  make nm           - Build Name Server only"
	@echo "  make ss           - Build Storage Server only"
	@echo "  make client       - Build Client only"
	@echo ""
	@echo "Clean Targets:"
	@echo "  make clean        - Clean all build artifacts"
	@echo "  make clean-nm     - Clean Name Server only"
	@echo "  make clean-ss     - Clean Storage Server only"
	@echo "  make clean-client - Clean Client only"
	@echo ""
	@echo "Install Target:"
	@echo "  make install      - Install binaries to bin/ directory"
	@echo ""
	@echo "Run Targets (for testing):"
	@echo "  make run-nm       - Run Name Server"
	@echo "  make run-ss1      - Run Storage Server 1"
	@echo "  make run-ss2      - Run Storage Server 2"
	@echo "  make run-client   - Run Client"
	@echo ""
	@echo "Other Targets:"
	@echo "  make help         - Show this help message"
	@echo ""

# Rebuild everything from scratch
.PHONY: rebuild
rebuild: clean all
	@echo "✓ Rebuild complete"

# Dependencies (auto-generated)
-include $(NM_OBJ:.o=.d)
-include $(SS_OBJ:.o=.d)
-include $(CLIENT_OBJ:.o=.d)
-include $(COMMON_OBJ:.o=.d)
