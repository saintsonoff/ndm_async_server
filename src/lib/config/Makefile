# lib/config/Makefile - Config library build system
# Builds libconfig.a - Configuration management library

# Use exported variables from parent Makefile
INCLUDE_DIR := $(BUILD_DIR)/include
LOCAL_INCLUDE_DIR := $(INCLUDE_DIR)/config
CXXFLAGS := $(CXX_FLAGS) -I$(INCLUDE_DIR)
LOCAL_OBJS_DIR := $(OBJS_DIR)/lib/config
LIB_TARGET := $(LIB_DIR)/libconfig.a

# Source files
SOURCES := config.cpp
OBJECTS := $(SOURCES:%.cpp=$(LOCAL_OBJS_DIR)/%.o)
HEADERS := config.hpp
INSTALLED_HEADERS := $(HEADERS:%.hpp=$(LOCAL_INCLUDE_DIR)/%.hpp)

.PHONY: all clean

# Default target
all: $(INSTALLED_HEADERS) $(LIB_TARGET)

# Build static library
$(LIB_TARGET): $(OBJECTS) | $(LIB_DIR)
	@echo "Creating libconfig.a..."
	ar rcs $@ $^

# Compile sources
$(LOCAL_OBJS_DIR)/%.o: %.cpp $(HEADERS) | $(LOCAL_OBJS_DIR)
	@echo "Compiling config: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Copy headers to build/include
$(LOCAL_INCLUDE_DIR)/%.hpp: %.hpp | $(LOCAL_INCLUDE_DIR)
	@echo "Installing header: $<"
	@cp $< $@

# Create directories
$(LOCAL_OBJS_DIR):
	@mkdir -p $(LOCAL_OBJS_DIR)

$(LIB_DIR):
	@mkdir -p $(LIB_DIR)

$(LOCAL_INCLUDE_DIR):
	@mkdir -p $(LOCAL_INCLUDE_DIR)

# Clean
clean:
	@echo "Cleaning config..."
	rm -rf $(LOCAL_OBJS_DIR)
	@if [ -d $(LOCAL_INCLUDE_DIR) ]; then rm -rf $(LOCAL_INCLUDE_DIR); fi
	rm -f $(LIB_TARGET)
