# Makefile for CIS427_PA1 project
CC = g++
CCOMPILER = gcc
TARGET = server
CLIENT = client
OBJS = server.o utils.o sqlite3.o
CLIENT_OBJS = client.o

# Flags
CFLAGS = -Wall
CXXFLAGS = -Wall
LDFLAGS = -lpthread -ldl

# Default target
all: $(TARGET) $(CLIENT)

# Build the server
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

# Build the client
$(CLIENT): $(CLIENT_OBJS)
	$(CC) -o $@ $(CLIENT_OBJS)

# Compile SQLite as C (not C++)
sqlite3.o: sqlite3.c sqlite3.h
	$(CCOMPILER) -c sqlite3.c -o sqlite3.o

# Compile server C++ files
server.o: server.cpp utils.hpp
	$(CC) -c server.cpp -o server.o

utils.o: utils.cpp utils.hpp
	$(CC) -c utils.cpp -o utils.o

# Compile client
client.o: client.cpp
	$(CC) -c client.cpp -o client.o

# Clean up
clean:
	rm -f $(OBJS) $(CLIENT_OBJS) $(TARGET) $(CLIENT) *.o

# Run the server (optional)
run: $(TARGET)
	./$(TARGET)

# Phony targets
.PHONY: all clean run