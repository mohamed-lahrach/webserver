# Main webserver project
NAME = webserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g

SRC = main.cpp Server_setup/server.cpp Server_setup/util_server.cpp  \
      Server_setup/socket.cpp Server_setup/non_blocking.cpp client/client.cpp \
      request/request.cpp request/get_handler.cpp request/post_handler.cpp \
      request/delete_handler.cpp response/response.cpp utils/utils.cpp \
      utils/mime_types.cpp config/parser.cpp

OBJ = $(SRC:.cpp=.o)

# Debug testing target
DEBUG_TARGET = debug
DEBUG_CXXFLAGS = -Wall -Wextra -std=c++98 -g -O0 -DDEBUG
LEXER_SRC = config/Lexer.cpp config/parser.cpp
TEST_SRC = testing_main.cpp

# Default target
all: $(NAME)

# Main webserver build
$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJ)

# Debug build target
$(DEBUG_TARGET): $(LEXER_SRC) $(TEST_SRC)
	$(CXX) $(DEBUG_CXXFLAGS) -o $@ $^

# Clean object files
clean:
	rm -f $(OBJ)

# Full clean
fclean: clean
	rm -f $(NAME) $(DEBUG_TARGET)

# Rebuild
re: fclean all

# Run debug
run: $(DEBUG_TARGET)
	./$(DEBUG_TARGET)

# Debug with GDB
gdb: $(DEBUG_TARGET)
	gdb ./$(DEBUG_TARGET)

.PHONY: all clean fclean re run gdb
