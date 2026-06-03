# Colors
RESET	= "\033[0m"
BLACK	= "\033[30m"
RED		= "\033[31m"
GREEN	= "\033[32m"
YELLOW	= "\033[33m"
BLUE	= "\033[34m"
MAGENTA	= "\033[35m"
CYAN	= "\033[36m"
WHITE	= "\033[37m"

# Emojis
SUCCESS	= ✅
BUILD	= 🔨
CLEAN	= 🧹
FCLEAN	= 🧼

# Compiler
NAME		= webserv
CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98 -I.

# Sources
SRC_DIR		= .
SRCS		= main.cpp \
			  Tokenizer.cpp \
			  ConfigParser.cpp \
			  ServerConfig.cpp \
			  LocationConfig.cpp \
			  Server.cpp \
			  SocketUtils.cpp \
			  Client.cpp \
			  Listener.cpp \
			  HttpRequest.cpp \
			  HttpRequestParser.cpp \
			  RouteDecision.cpp \
			  Router.cpp \
			  ResponseBuilder.cpp \
			  MimeTypes.cpp

# Objects
OBJ_DIR		= obj
OBJS		= $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))

#Headers
HEADERS	= Tokenizer.hpp \
			  ConfigParser.hpp \
			  ServerConfig.hpp \
			  LocationConfig.hpp \
			  Server.hpp \
			  SocketUtils.hpp \
			  Client.hpp \
			  Listener.hpp \
			  HttpRequest.hpp \
			  HttpRequestParser.hpp \
			  RouteDecision.hpp \
			  Router.hpp \
			  ResponseBuilder.hpp \
			  MimeTypes.hpp

all: $(NAME)

$(NAME): $(OBJS)
	@echo $(BUILD) $(RED) "Compiling $(NAME)..." $(RESET)
	@$(CXX) $(CXXFLAGS) -o $@ $^
	@echo $(SUCCESS) $(GREEN) "Compiling $(NAME) FINISHED" $(RESET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@mkdir -p $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(OBJ_DIR)
	@echo $(CLEAN) $(YELLOW) "Cleaned!" $(RESET)
fclean: clean
	@rm -f $(NAME)
	@echo $(FCLEAN) $(YELLOW) "Full Cleaned!" $(RESET)

re: fclean all

.PHONY: all clean fclean re
