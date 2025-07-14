NAME = webserv
SIMPLE_TEST = simple_test
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRC = main.cpp \
      config/parser.cpp \
      server/server.cpp \
      # Add other .cpp files here

SIMPLE_TEST_SRC = simple_test.cpp \
                  config/Lexer.cpp

OBJ = $(SRC:.cpp=.o)
SIMPLE_TEST_OBJ = $(SIMPLE_TEST_SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJ)

# Simple test target
simple-test: $(SIMPLE_TEST)

$(SIMPLE_TEST): $(SIMPLE_TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $(SIMPLE_TEST) $(SIMPLE_TEST_OBJ)

# Run simple test
run-simple-test: $(SIMPLE_TEST)
	./$(SIMPLE_TEST)

clean:
	rm -f $(OBJ) $(SIMPLE_TEST_OBJ)

fclean: clean
	rm -f $(NAME) $(SIMPLE_TEST)

re: fclean all

.PHONY: all simple-test run-simple-test clean fclean re
