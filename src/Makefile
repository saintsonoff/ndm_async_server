EXEC_TARGET := async_server

CXX := g++
CC := gcc

BUILD_TYPE ?= release

COMMON_FLAGS := -std=c++23 \
                -Wall \
                -Wextra \
                -Wpedantic \
                -Wcast-align \
                -Wcast-qual \
                -Wconversion \
                -Wctor-dtor-privacy \
                -Wduplicated-branches \
                -Wduplicated-cond \
                -Wextra-semi \
                -Wfloat-equal \
                -Wnon-virtual-dtor \
                -Wold-style-cast \
                -Woverloaded-virtual \
                -Wredundant-decls \
                -Wsign-conversion \
                -Wsign-promo \
                -Wstrict-null-sentinel \
                -Wswitch-default \
                -Wswitch-enum \
                -Wundef \
                -Wunused \
                -Wformat=2 \
                -Wnull-dereference \
                -Wdouble-promotion \
                -Wimplicit-fallthrough

ifeq ($(BUILD_TYPE),debug)
    CXX_FLAGS := $(COMMON_FLAGS) \
                 -O0 \
                 -g3 \
                 -ggdb \
                 -fno-omit-frame-pointer \
                 -fsanitize=address \
                 -fsanitize=undefined \
                 -DDEBUG
    LDFLAGS := -fsanitize=address -fsanitize=undefined
else ifeq ($(BUILD_TYPE),release)
    CXX_FLAGS := $(COMMON_FLAGS) \
                 -O3 \
                 -march=native \
                 -mtune=native \
                 -flto \
                 -fomit-frame-pointer \
                 -DNDEBUG \
                 -Werror
    LDFLAGS := -flto -Wl,--as-needed -Wl,-O2
else ifeq ($(BUILD_TYPE),relwithdebinfo)
    CXX_FLAGS := $(COMMON_FLAGS) \
                 -O2 \
                 -g \
                 -march=native \
                 -DNDEBUG
    LDFLAGS := -Wl,--as-needed
else
    $(error Invalid BUILD_TYPE: $(BUILD_TYPE). Use: debug, release, or relwithdebinfo)
endif

CC_FLAGS := -Wall -Wextra

BUILD_DIR := $(CURDIR)/build
OBJS_DIR := $(BUILD_DIR)/objs
LIB_DIR := $(BUILD_DIR)/lib
BIN_DIR := $(BUILD_DIR)/bin

export CXX CC CXX_FLAGS CC_FLAGS LDFLAGS BUILD_DIR OBJS_DIR LIB_DIR BIN_DIR

SUBDIRS := lib bin

.PHONY: all $(SUBDIRS) clean install help

all: $(SUBDIRS)

lib:
	@echo "Building library..."
	$(MAKE) -C lib

bin: lib
	@echo "Building binary..."
	$(MAKE) -C bin

clean:
	@echo "Cleaning src build artifacts..."
	rm -rf $(BUILD_DIR)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

install: all
	@echo "Installing $(EXEC_TARGET) to /usr/local/bin..."
	@mkdir -p /usr/local/bin
	@cp -f $(BIN_DIR)/$(EXEC_TARGET) /usr/local/bin/
	@chmod 755 /usr/local/bin/$(EXEC_TARGET)
	@echo "Installation complete!"

help:
	@echo "src/Makefile - Source Build System"
	@echo "==================================="
	@echo ""
	@echo "Build Types:"
	@echo "  BUILD_TYPE=debug          - Debug build with sanitizers (-O0 -g3 -fsanitize=address,undefined)"
	@echo "  BUILD_TYPE=release        - Release build with optimizations (-O3 -march=native -flto) [default]"
	@echo "  BUILD_TYPE=relwithdebinfo - Release with debug info (-O2 -g -march=native)"
	@echo ""
	@echo "Usage:"
	@echo "  make                      - Build with release settings"
	@echo "  make BUILD_TYPE=debug     - Build with debug settings"
	@echo "  make BUILD_TYPE=release   - Build with release settings"
	@echo ""
	@echo "Targets:"
	@echo "  make         - Build library and binary (default)"
	@echo "  make lib     - Build only the library"
	@echo "  make bin     - Build only the binary (requires lib)"
	@echo "  make clean   - Remove all build artifacts"
	@echo "  make install - Install binary to /usr/local/bin"
	@echo "  make help    - Show this help"