EXECUTABLE_NAME := async_server
INCLUDE_DIRECTORY := $(BUILD_DIR)/include
CXXFLAGS := $(CXX_FLAGS) -I$(INCLUDE_DIRECTORY)
LOCAL_OBJECTS_DIRECTORY := $(OBJS_DIR)/bin
EXECUTABLE_PATH := $(BIN_DIR)/$(EXECUTABLE_NAME)

LIBRARIES := $(LIB_DIR)/libcomponent_manager.a \
             $(LIB_DIR)/libtcp_listener.a \
             $(LIB_DIR)/libudp_listener.a \
             $(LIB_DIR)/libevent_loop.a \
             $(LIB_DIR)/libcommand.a \
             $(LIB_DIR)/libsocket.a \
             $(LIB_DIR)/libconfig.a

SOURCES := main.cpp
OBJECTS := $(SOURCES:%.cpp=$(LOCAL_OBJECTS_DIRECTORY)/%.o)

.PHONY: all clean

all: $(EXECUTABLE_PATH)

$(EXECUTABLE_PATH): $(OBJECTS) $(LIBRARIES) | $(BIN_DIR)
	@echo "Linking executable: $@"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBRARIES)
	@echo "Built $(EXECUTABLE_NAME) successfully!"

$(LOCAL_OBJECTS_DIRECTORY)/%.o: %.cpp | $(LOCAL_OBJECTS_DIRECTORY)
	@echo "Compiling: $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(LOCAL_OBJECTS_DIRECTORY):
	@mkdir -p $(LOCAL_OBJECTS_DIRECTORY)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(LIB_DIR)/%.a:
	@echo "Library $@ not found, building libraries..."
	@$(MAKE) -C ../lib

clean:
	@echo "Cleaning bin build artifacts..."
	@rm -rf $(LOCAL_OBJECTS_DIRECTORY)
	@rm -f $(EXECUTABLE_PATH)