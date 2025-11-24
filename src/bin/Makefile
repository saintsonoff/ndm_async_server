# src/bin/Makefile - Binary build system
# Builds the async_server executable

# Use exported variables from parent Makefile
# CXX, CXX_FLAGS, BUILD_DIR, OBJS_DIR, LIB_DIR, BIN_DIR are set by parent

# Binary-specific settings
TARGET := async_server
INCLUDE_DIR := $(BUILD_DIR)/include
CXXFLAGS := $(CXX_FLAGS) -I$(INCLUDE_DIR)
LOCAL_OBJS_DIR := $(OBJS_DIR)/bin
TARGET_PATH := $(BIN_DIR)/$(TARGET)

# Libraries to link against (order matters!)
LIBS := $(LIB_DIR)/libserver.a \
        $(LIB_DIR)/libcommand.a \
        $(LIB_DIR)/libsocket.a \
        $(LIB_DIR)/libconfig.a

# Source files in this directory
SOURCES := main.cpp
OBJECTS := $(SOURCES:%.cpp=$(LOCAL_OBJS_DIR)/%.o)

.PHONY: all clean

# Default target: build the binary
all: $(TARGET_PATH)

# Build the executable
$(TARGET_PATH): $(OBJECTS) $(LIBS) | $(BIN_DIR)
	@echo "Linking executable: $@"
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LIBS)
	@echo "Built $(TARGET) successfully!"

# Compile source files to object files
$(LOCAL_OBJS_DIR)/%.o: %.cpp | $(LOCAL_OBJS_DIR)
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create necessary directories
$(LOCAL_OBJS_DIR):
	@mkdir -p $(LOCAL_OBJS_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Ensure libraries are built (this is just a safety check)
$(LIB_DIR)/%.a:
	@echo "Library $@ not found, building libraries..."
	$(MAKE) -C ../lib

# Clean this directory's build artifacts
clean:
	@echo "Cleaning bin build artifacts..."
	rm -rf $(LOCAL_OBJS_DIR)
	rm -f $(TARGET_PATH)