# src/lib/Makefile - Libraries orchestration
# Coordinates building of all library modules

# Use exported variables from parent Makefile
# CXX, CXX_FLAGS, BUILD_DIR, OBJS_DIR, LIB_DIR are set by parent

# Re-export for subdirectories
export CXX CXX_FLAGS BUILD_DIR OBJS_DIR LIB_DIR

# Subdirectories (order matters for dependencies)
SUBDIRS := config socket command server

.PHONY: all $(SUBDIRS) clean

# Default target: build all libraries in order
all: $(SUBDIRS)

# Build each library
config:
	@echo "Building config library..."
	$(MAKE) -C config

socket:
	@echo "Building socket library..."
	$(MAKE) -C socket

command:
	@echo "Building command library..."
	$(MAKE) -C command

server: config socket command
	@echo "Building server library..."
	$(MAKE) -C server

# Clean all libraries
clean:
	@echo "Cleaning all libraries..."
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done