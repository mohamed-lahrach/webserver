
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++98 -g -O0 -DDEBUG

# Source files
LEXER_SRC = config/Lexer.cpp config/parser.cpp
TEST_SRC = testing_main.cpp
TARGET = debug

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(LEXER_SRC) $(TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^
# Clean
clean:
	rm -f $(TARGET)

# Run
run: $(TARGET)
	./$(TARGET)

# Debug with GDB
gdb: $(TARGET)
	gdb ./$(TARGET)

.PHONY: all clean run gdb