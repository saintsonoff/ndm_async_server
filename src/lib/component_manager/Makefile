INCLUDE_DIRECTORY := $(BUILD_DIR)/include
LOCAL_INCLUDE_DIRECTORY := $(INCLUDE_DIRECTORY)/component_manager
CXXFLAGS := $(CXX_FLAGS) -I$(INCLUDE_DIRECTORY)
LOCAL_OBJECTS_DIRECTORY := $(OBJS_DIR)/lib/component_manager
LIBRARY_TARGET := $(LIB_DIR)/libcomponent_manager.a

SOURCES := component_manager_builder.cpp component_manager.cpp
OBJECTS := $(SOURCES:%.cpp=$(LOCAL_OBJECTS_DIRECTORY)/%.o)
HEADERS := component_manager_builder.hpp component_manager.hpp
INSTALLED_HEADERS := $(HEADERS:%.hpp=$(LOCAL_INCLUDE_DIRECTORY)/%.hpp)

.PHONY: all clean

all: $(INSTALLED_HEADERS) $(LIBRARY_TARGET)

$(LIBRARY_TARGET): $(OBJECTS) | $(LIB_DIR)
	@echo "Creating libcomponent_manager.a..."
	@ar rcs $@ $^

$(LOCAL_OBJECTS_DIRECTORY)/%.o: %.cpp $(HEADERS) | $(LOCAL_OBJECTS_DIRECTORY)
	@echo "Compiling component_manager: $<"
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
	@echo "Cleaning component_manager..."
	@rm -rf $(LOCAL_OBJECTS_DIRECTORY)
	@if [ -d $(LOCAL_INCLUDE_DIRECTORY) ]; then rm -rf $(LOCAL_INCLUDE_DIRECTORY); fi
	@rm -f $(LIBRARY_TARGET)
