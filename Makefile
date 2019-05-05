SRCDIR   = src
BINDIR   = bin
INCLUDES = include

CXX ?=g++
CXXFLAGS=-Wall -Wextra -g -fno-stack-protector -z execstack -lpthread -std=c++17 -m32 -I $(INCLUDES)/

# For development
# CXX = clang++
# CXXFLAGS = -Weverything -Wno-c++98-compat -Wno-padded -g -std=c++17 -pedantic -fsanitize=address,undefined -I $(INCLUDES)/
# LDFLAGS = -g -lpthread -lstdc++ -fsanitize=address,undefined

CLIENT = $(BINDIR)/client
CLIENT_OBJS = src/client.o src/ring_buffer.o src/client_manager.o \
	src/grass_exception.o src/network.o src/filesystem.o

SERVER = $(BINDIR)/server
SERVER_OBJS = src/server.o src/session.o src/config.o src/server_manager.o \
	src/ring_buffer.o src/grass_exception.o src/network.o src/filetransfer.o \
	src/filesystem.o src/grep.o src/grass.o

.SUFFIXES: .cpp .o

all: $(CLIENT) $(SERVER)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(CLIENT): $(CLIENT_OBJS)
	$(CXX) $^ -o $@ $(CXXFLAGS)

$(SERVER): $(SERVER_OBJS)
	$(CXX) $^ -o $@ $(CXXFLAGS)

.PHONY: clean
clean:
	rm -f $(CLIENT) $(SERVER) $(CLIENT_OBJS) $(SERVER_OBJS)
