# Makefile for MBP-10 Order Book Reconstruction Tool
# Windows-compatible version with fallbacks for Unix commands

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O3
DEBUGFLAGS = -g -O0 -DDEBUG
INCLUDES = -Iinclude
LIBS = 

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
TESTDIR = tests
BENCHDIR = benchmarks

# Output binary name
TARGET = reconstruction_optimal.exe

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
HEADERS = $(wildcard $(INCDIR)/*.hpp)

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/*.cpp)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/%.cpp=$(OBJDIR)/%.o)
TEST_TARGET = run_tests.exe

# Benchmark files
BENCH_SOURCES = $(wildcard $(BENCHDIR)/*.cpp)
BENCH_OBJECTS = $(BENCH_SOURCES:$(BENCHDIR)/%.cpp=$(OBJDIR)/%.o)
BENCH_TARGET = run_benchmarks.exe

# Detect OS - Windows doesn't have uname, so we'll use a different approach
ifdef OS
    # We're on Windows
    RM = del /Q
    RMDIR = rmdir /S /Q
    MKDIR = mkdir
    EXE_EXT = .exe
    PATH_SEP = \\
else
    # We're on Unix-like system
    RM = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
    EXE_EXT = 
    PATH_SEP = /
    
    # Platform-specific optimizations for Unix
    UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
    ifeq ($(UNAME_S),Linux)
        CXXFLAGS += -pthread -march=native -mtune=native
        LIBS += -lpthread
    endif
    ifeq ($(UNAME_S),Darwin)
        CXXFLAGS += -stdlib=libc++ -march=native -mtune=native
    endif
endif

# Build modes
.PHONY: all release debug clean test benchmark help

# Default target
all: release

# Release build (optimized for performance)
release: CXXFLAGS += -DNDEBUG -flto -ffast-math -funroll-loops
release: $(TARGET)

# Debug build (with debugging symbols and no optimization)
debug: CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic $(DEBUGFLAGS) $(INCLUDES)
debug: $(TARGET)

# Production build (maximum optimization)
production: CXXFLAGS += -DNDEBUG -flto -ffast-math -funroll-loops -fomit-frame-pointer -finline-functions
production: $(TARGET)

# Main target
$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CXX) $(OBJECTS) -o $@ $(LIBS)
	@echo "Build completed: $(TARGET)"
	@echo "Usage: ./$(TARGET) input.csv [output.csv]"

# Object files compilation
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS) | $(OBJDIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Create object directory
$(OBJDIR):
ifdef OS
	@if not exist "$(OBJDIR)" $(MKDIR) "$(OBJDIR)"
else
	@$(MKDIR) $(OBJDIR)
endif

# Test compilation and execution
test: $(TEST_TARGET)
	@echo "Running tests..."
	./$(TEST_TARGET)

$(TEST_TARGET): $(filter-out $(OBJDIR)/main.o, $(OBJECTS)) $(TEST_OBJECTS) | $(OBJDIR)
	@echo "Building tests..."
	$(CXX) $(filter-out $(OBJDIR)/main.o, $(OBJECTS)) $(TEST_OBJECTS) -o $@ $(LIBS)

$(OBJDIR)/%.o: $(TESTDIR)/%.cpp $(HEADERS) | $(OBJDIR)
	@echo "Compiling test $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Benchmark compilation and execution
benchmark: $(BENCH_TARGET)
	@echo "Running benchmarks..."
	./$(BENCH_TARGET)

$(BENCH_TARGET): $(filter-out $(OBJDIR)/main.o, $(OBJECTS)) $(BENCH_OBJECTS) | $(OBJDIR)
	@echo "Building benchmarks..."
	$(CXX) $(filter-out $(OBJDIR)/main.o, $(OBJECTS)) $(BENCH_OBJECTS) -o $@ $(LIBS)

$(OBJDIR)/%.o: $(BENCHDIR)/%.cpp $(HEADERS) | $(OBJDIR)
	@echo "Compiling benchmark $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts - Windows compatible
clean:
	@echo "Cleaning build artifacts..."
ifdef OS
	@if exist "$(OBJDIR)" $(RMDIR) "$(OBJDIR)" 2>nul || echo Object directory cleaned
	@if exist "$(TARGET)" $(RM) "$(TARGET)" 2>nul || echo Target cleaned
	@if exist "$(TEST_TARGET)" $(RM) "$(TEST_TARGET)" 2>nul || echo Test target cleaned
	@if exist "$(BENCH_TARGET)" $(RM) "$(BENCH_TARGET)" 2>nul || echo Benchmark target cleaned
	@if exist "*.csv.out" $(RM) "*.csv.out" 2>nul || echo CSV outputs cleaned
	@if exist "*.log" $(RM) "*.log" 2>nul || echo Logs cleaned
else
	@$(RMDIR) $(OBJDIR) 2>/dev/null || true
	@$(RM) $(TARGET) $(TEST_TARGET) $(BENCH_TARGET) 2>/dev/null || true
	@$(RM) *.csv.out *.log *.prof 2>/dev/null || true
endif
	@echo "Clean completed"

# Quick test with sample data
quick-test: release
	@echo "Running quick test..."
ifdef OS
	@if exist "mbo.csv" ($(TARGET) mbo.csv test_output.csv && echo "Quick test completed. Check test_output.csv") else (echo "Error: mbo.csv not found for quick test")
else
	@if [ -f "mbo.csv" ]; then \
		./$(TARGET) mbo.csv test_output.csv; \
		echo "Quick test completed. Check test_output.csv"; \
	else \
		echo "Error: mbo.csv not found for quick test"; \
	fi
endif

# Performance test
perf-test: release
	@echo "Running performance test..."
ifdef OS
	@if exist "mbo.csv" (echo "Starting performance test..." && $(TARGET) mbo.csv perf_output.csv && echo "Performance test completed") else (echo "Error: mbo.csv not found for performance test")
else
	@if [ -f "mbo.csv" ]; then \
		time ./$(TARGET) mbo.csv perf_output.csv; \
		echo "Performance test completed"; \
	else \
		echo "Error: mbo.csv not found for performance test"; \
	fi
endif

# Help target
help:
	@echo "Available targets:"
	@echo "  all         - Build release version (default)"
	@echo "  release     - Build optimized release version"
	@echo "  debug       - Build debug version with symbols"
	@echo "  production  - Build with maximum optimizations"
	@echo "  test        - Build and run unit tests"
	@echo "  benchmark   - Build and run performance benchmarks"
	@echo "  clean       - Remove all build artifacts"
	@echo "  quick-test  - Run quick test with mbo.csv"
	@echo "  perf-test   - Run performance test"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Build configuration:"
	@echo "  CXX=$(CXX)"
	@echo "  CXXFLAGS=$(CXXFLAGS)"
	@echo "  TARGET=$(TARGET)"

# Special targets that don't correspond to files
.PHONY: all release debug production clean test benchmark quick-test perf-test help