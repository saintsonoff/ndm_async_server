# Root Makefile - Async Server Project
# This file orchestrates the recursive build process

.PHONY: all clean test install help

# Default target
all:
	@echo "Building Async Server..."
	$(MAKE) -C src

# Clean all build artifacts
clean:
	@echo "Cleaning build artifacts..."
	$(MAKE) -C src clean
	@if [ -d tests ]; then $(MAKE) -C tests clean; fi

# Run tests
test:
	@echo "Running tests..."
	@if [ -d tests ]; then \
		$(MAKE) -C tests test; \
	else \
		echo "No tests directory found!"; \
		exit 1; \
	fi

# Install the binary (optional)
install:
	@echo "Installing async_server..."
	$(MAKE) -C src install

# Help message
help:
	@echo "Async Server Build System"
	@echo "========================="
	@echo ""
	@echo "Available targets:"
	@echo "  make         - Build the entire project (default)"
	@echo "  make clean   - Remove all build artifacts"
	@echo "  make test    - Run all tests"
	@echo "  make install - Install the binary to /usr/local/bin"
	@echo "  make help    - Show this help message"
	@echo ""
	@echo "Directory structure:"
	@echo "  src/         - Source code with recursive Makefiles"
	@echo "  src/lib/     - Library code"
	@echo "  src/bin/     - Binary/executable code"
	@echo "  tests/       - Test suite"
