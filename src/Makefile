# src/Makefile - Source directory build orchestration
# This Makefile coordinates the build of library and binary subdirectories

EXEC_TARGET := async_server

# Compiler settings
CXX := g++
CC := gcc
CXX_FLAGS := -std=c++23 -Wall -Wextra
CC_FLAGS := -Wall -Wextra

# Build directory structure
BUILD_DIR := $(CURDIR)/build
OBJS_DIR := $(BUILD_DIR)/objs
LIB_DIR := $(BUILD_DIR)/lib
BIN_DIR := $(BUILD_DIR)/bin

# Export variables for subdirectories
export CXX CC CXX_FLAGS CC_FLAGS BUILD_DIR OBJS_DIR LIB_DIR BIN_DIR

# Subdirectories to build (order matters: lib before bin)
SUBDIRS := lib bin

.PHONY: all $(SUBDIRS) clean install help

# Default target: build all subdirectories
all: $(SUBDIRS)

# Build each subdirectory
lib:
	@echo "Building library..."
	$(MAKE) -C lib

bin: lib
	@echo "Building binary..."
	$(MAKE) -C bin

# Clean all build artifacts
clean:
	@echo "Cleaning src build artifacts..."
	rm -rf $(BUILD_DIR)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Install the binary
install: all
	@echo "Installing $(EXEC_TARGET) to /usr/local/bin..."
	@mkdir -p /usr/local/bin
	@cp -f $(BIN_DIR)/$(EXEC_TARGET) /usr/local/bin/
	@chmod 755 /usr/local/bin/$(EXEC_TARGET)
	@echo "Installation complete!"

# Help message
help:
	@echo "src/Makefile - Source Build System"
	@echo "==================================="
	@echo ""
	@echo "Targets:"
	@echo "  make         - Build library and binary (default)"
	@echo "  make lib     - Build only the library"
	@echo "  make bin     - Build only the binary (requires lib)"
	@echo "  make clean   - Remove all build artifacts"
	@echo "  make install - Install binary to /usr/local/bin"
	@echo "  make help    - Show this help"