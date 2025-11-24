# Command library build system

INCLUDE_DIR := $(BUILD_DIR)/include
LOCAL_INCLUDE_DIR := $(INCLUDE_DIR)/command
CXXFLAGS := $(CXX_FLAGS) -I$(INCLUDE_DIR)
LOCAL_OBJS_DIR := $(OBJS_DIR)/lib/command
LIB_TARGET := $(LIB_DIR)/libcommand.a

SOURCES := command.cpp
OBJECTS := $(SOURCES:%.cpp=$(LOCAL_OBJS_DIR)/%.o)
HEADERS := command.hpp
INSTALLED_HEADERS := $(HEADERS:%.hpp=$(LOCAL_INCLUDE_DIR)/%.hpp)

.PHONY: all clean

all: $(INSTALLED_HEADERS) $(LIB_TARGET)

$(LIB_TARGET): $(OBJECTS) | $(LIB_DIR)
	@echo "Creating libcommand.a..."
	ar rcs $@ $^

$(LOCAL_OBJS_DIR)/%.o: %.cpp $(HEADERS) | $(LOCAL_OBJS_DIR)
	@echo "Compiling command: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LOCAL_INCLUDE_DIR)/%.hpp: %.hpp | $(LOCAL_INCLUDE_DIR)
	@echo "Installing header: $<"
	@cp $< $@

$(LOCAL_OBJS_DIR):
	@mkdir -p $(LOCAL_OBJS_DIR)

$(LIB_DIR):
	@mkdir -p $(LIB_DIR)

$(LOCAL_INCLUDE_DIR):
	@mkdir -p $(LOCAL_INCLUDE_DIR)

clean:
	@echo "Cleaning command..."
	rm -rf $(LOCAL_OBJS_DIR)
	@if [ -d $(LOCAL_INCLUDE_DIR) ]; then rm -rf $(LOCAL_INCLUDE_DIR); fi
	rm -f $(LIB_TARGET)
