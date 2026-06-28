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
INC_DIR		= includes
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98 -I$(INC_DIR)

# Sources
SRC_DIR		= srcs
SRCS		= main.cpp \
			  config/Tokenizer.cpp \
			  config/ConfigParser.cpp \
			  config/ServerConfig.cpp \
			  config/LocationConfig.cpp \
			  network/SocketUtils.cpp \
			  network/Listener.cpp \
			  core/Server.cpp \
			  core/Client.cpp \
			  http/HttpRequest.cpp \
			  http/HttpRequestParser.cpp \
			  http/RouteDecision.cpp \
			  http/Router.cpp \
			  http/PathUtils.cpp \
			  response/ResponseBuilder.cpp \
			  response/MimeTypes.cpp \
			  upload/UploadHandler.cpp \
			  cgi/CgiSession.cpp

# Objects
OBJ_DIR		= obj
OBJS		= $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))

# Headers
HEADERS		= $(wildcard $(INC_DIR)/*.hpp)

all: $(NAME)

$(NAME): $(OBJS)
	@echo $(BUILD) $(RED) "Compiling $(NAME)..." $(RESET)
	@$(CXX) $(CXXFLAGS) -o $@ $^
	@echo $(SUCCESS) $(GREEN) "Compiling $(NAME) FINISHED" $(RESET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(OBJ_DIR)
	@echo $(CLEAN) $(YELLOW) "Cleaned!" $(RESET)
fclean: clean
	@rm -f $(NAME)
	@echo $(FCLEAN) $(YELLOW) "Full Cleaned!" $(RESET)

re: fclean all

.PHONY: all clean fclean re
