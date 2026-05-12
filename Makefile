NAME		= webserv

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98 -I.

SRC_DIR		= .
OBJ_DIR		= obj

SRCS		= main.cpp \
			  Tokenizer.cpp \
			  ConfigParser.cpp \
			  ServerConfig.cpp \
			  LocationConfig.cpp

OBJS		= $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
