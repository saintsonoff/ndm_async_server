# Libraries orchestration

export CXX CXX_FLAGS BUILD_DIR OBJS_DIR LIB_DIR

SUBDIRS := config socket command component event_loop tcp_listener udp_listener component_manager

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

component:
	@echo "Building component library..."
	$(MAKE) -C component

event_loop: component
	@echo "Building event_loop library..."
	$(MAKE) -C event_loop

tcp_listener: component event_loop socket command
	@echo "Building tcp_listener library..."
	$(MAKE) -C tcp_listener

udp_listener: component event_loop socket command
	@echo "Building udp_listener library..."
	$(MAKE) -C udp_listener

component_manager: config socket command component event_loop tcp_listener udp_listener
	@echo "Building component_manager library..."
	$(MAKE) -C component_manager

clean:
	@echo "Cleaning all libraries..."
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done