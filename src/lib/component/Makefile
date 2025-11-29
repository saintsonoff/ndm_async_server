INCLUDE_DIRECTORY := $(BUILD_DIR)/include/component

.PHONY: all clean

all:
	@mkdir -p $(INCLUDE_DIRECTORY)
	@cp component_base.hpp $(INCLUDE_DIRECTORY)/
	@echo "Installing header: component_base.hpp"

clean:
	@rm -f $(INCLUDE_DIRECTORY)/component_base.hpp
