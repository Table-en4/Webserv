NAME = Webserv
CCXX = c++
CCXXFLAGS = -Wall -Werror -Wextra -std=c++98 -g3
SRCS = srcs/main.cpp \
	srcs/Parser.cpp srcs/LocationConfig.cpp srcs/ServerConfig.cpp \
	srcs/ServerManager.cpp srcs/HttpRequest.cpp srcs/HttpRespons.cpp \
	srcs/Routes.cpp srcs/CgiHandler.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
		$(CCXX) $(CCXXFLAGS) -o $(NAME) $(OBJS)

clean:
		rm -f $(OBJS)

fclean: clean
		rm -f $(NAME)

re: fclean all

.PHONY: clean fclean all re
