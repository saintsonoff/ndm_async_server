# tests/Makefile - Test suite management
# Manages Python-based end-to-end tests

PYTHON := python3
PYTEST := $(PYTHON) -m pytest
TEST_FILES := test_server.py test_property_based.py
REQUIREMENTS := requirements.txt

.PHONY: all test clean install-deps help

# Default target: run all tests
all: test

# Run the test suite
test:
	@echo "Running test suite..."
	@if [ ! -f $(REQUIREMENTS) ]; then \
		echo "Error: $(REQUIREMENTS) not found!"; \
		exit 1; \
	fi
	@if [ -f ../.env ]; then \
		export $$(cat ../.env | grep -v '^#' | xargs) && $(PYTEST) $(TEST_FILES) -v; \
	else \
		$(PYTEST) $(TEST_FILES) -v; \
	fi

# Run tests with coverage
test-coverage:
	@echo "Running tests with coverage..."
	$(PYTEST) $(TEST_FILES) --cov=../src --cov-report=html --cov-report=term

# Run only TCP tests
test-tcp:
	@echo "Running TCP tests..."
	$(PYTEST) $(TEST_FILES) -v -k "tcp"

# Run only UDP tests
test-udp:
	@echo "Running UDP tests..."
	$(PYTEST) $(TEST_FILES) -v -k "udp"

# Run only property-based tests
test-property:
	@echo "Running property-based tests..."
	$(PYTEST) test_property_based.py -v

# Install test dependencies
install-deps:
	@echo "Installing test dependencies..."
	$(PYTHON) -m pip install -r $(REQUIREMENTS)

# Clean test artifacts
clean:
	@echo "Cleaning test artifacts..."
	rm -rf .pytest_cache
	rm -rf __pycache__
	rm -rf .hypothesis
	rm -rf htmlcov
	rm -f .coverage

# Help message
help:
	@echo "tests/Makefile - Test Suite Management"
	@echo "======================================="
	@echo ""
	@echo "Targets:"
	@echo "  make              - Run all tests (default)"
	@echo "  make test         - Run all tests"
	@echo "  make test-tcp     - Run only TCP tests"
	@echo "  make test-udp     - Run only UDP tests"
	@echo "  make test-property- Run only property-based tests"
	@echo "  make test-coverage- Run tests with coverage report"
	@echo "  make install-deps - Install Python test dependencies"
	@echo "  make clean        - Remove test artifacts"
	@echo "  make help         - Show this help"
