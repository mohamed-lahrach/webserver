
NAME = webserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g

SRC = main.cpp Server_setup/server.cpp Server_setup/util_server.cpp  \
	Server_setup/socket.cpp Server_setup/non_blocking.cpp client/client.cpp \
	request/request.cpp request/get_handler.cpp request/post_handler.cpp \
	request/delete_handler.cpp response/response.cpp config/Lexer.cpp config/parser.cpp config/helper_functions.cpp \
	utils/mime_types.cpp 

OBJ = $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJ)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

run: all clean
	clear
	./$(NAME) ./test_configs/default.conf

.PHONY: all clean fclean re run
