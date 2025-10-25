# Compiler and flags
CXX      := clang++
CXXFLAGS := -std=c++17 -Wall -O2 -Iinclude

# Directories
SRC_DIR  := src
OBJ_DIR  := build
BIN_DIR  := bin

# Target executable name
TARGET   := $(BIN_DIR)/cache_sim

# Find all .cpp source files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
# Generate object file names under build/
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Default rule
all: $(TARGET)

# Link step
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "✅ Build complete → $(TARGET)"

# Compile step
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "🧹 Cleaned build directories."

# Run simulation
run: all
	./$(TARGET)
