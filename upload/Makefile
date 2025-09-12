NAME = webserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g3 -O0

SRC = main.cpp Server_setup/server.cpp Server_setup/util_server.cpp  \
	Server_setup/socket_setup.cpp Server_setup/epoll_setup.cpp client/client.cpp \
	request/request.cpp request/get_handler.cpp request/post_handler.cpp \
	request/delete_handler.cpp  request/post_handler_utils.cpp response/response.cpp config/Lexer.cpp config/parser.cpp config/helper_functions.cpp \
	utils/mime_types.cpp utils/utils.cpp cgi/cgi_runner.cpp 

OBJ = $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJ)

debug: CXXFLAGS += -DDEBUG
debug: $(NAME)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

run: all clean
	clear
	./$(NAME) ./test_configs/default.conf

leaks: all clean
	clear
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./$(NAME) ./test_configs/default.conf
.PHONY: all clean fclean re
