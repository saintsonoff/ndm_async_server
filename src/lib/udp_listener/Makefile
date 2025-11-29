INCLUDE_DIRECTORY := $(BUILD_DIR)/include
LOCAL_INCLUDE_DIRECTORY := $(INCLUDE_DIRECTORY)/udp_listener
CXXFLAGS := $(CXX_FLAGS) -I$(INCLUDE_DIRECTORY)
LOCAL_OBJECTS_DIRECTORY := $(OBJS_DIR)/lib/udp_listener
LIBRARY_TARGET := $(LIB_DIR)/libudp_listener.a

SOURCES := udp_listener_component.cpp
OBJECTS := $(SOURCES:%.cpp=$(LOCAL_OBJECTS_DIRECTORY)/%.o)
HEADERS := udp_listener_component.hpp
INSTALLED_HEADERS := $(HEADERS:%.hpp=$(LOCAL_INCLUDE_DIRECTORY)/%.hpp)

.PHONY: all clean

all: $(INSTALLED_HEADERS) $(LIBRARY_TARGET)

$(LIBRARY_TARGET): $(OBJECTS) | $(LIB_DIR)
	@echo "Creating libudp_listener.a..."
	@ar rcs $@ $^

$(LOCAL_OBJECTS_DIRECTORY)/%.o: %.cpp $(HEADERS) | $(LOCAL_OBJECTS_DIRECTORY)
	@echo "Compiling udp_listener: $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(LOCAL_INCLUDE_DIRECTORY)/%.hpp: %.hpp | $(LOCAL_INCLUDE_DIRECTORY)
	@echo "Installing header: $<"
	@cp $< $@

$(LOCAL_OBJECTS_DIRECTORY):
	@mkdir -p $(LOCAL_OBJECTS_DIRECTORY)

$(LIB_DIR):
	@mkdir -p $(LIB_DIR)

$(LOCAL_INCLUDE_DIRECTORY):
	@mkdir -p $(LOCAL_INCLUDE_DIRECTORY)

clean:
	@echo "Cleaning udp_listener..."
	@rm -rf $(LOCAL_OBJECTS_DIRECTORY)
	@if [ -d $(LOCAL_INCLUDE_DIRECTORY) ]; then rm -rf $(LOCAL_INCLUDE_DIRECTORY); fi
	@rm -f $(LIBRARY_TARGET)
