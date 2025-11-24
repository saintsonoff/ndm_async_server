# Libraries orchestration

export CXX CXX_FLAGS BUILD_DIR OBJS_DIR LIB_DIR

SUBDIRS := config socket command server

.PHONY: all $(SUBDIRS) clean

all: $(SUBDIRS)

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

clean:
	@echo "Cleaning all libraries..."
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done