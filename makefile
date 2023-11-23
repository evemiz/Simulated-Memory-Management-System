# Compiler
CC = g++

# Compiler flags
CFLAGS = -std=c++11 -Wall

# Executable name
TARGET = sim_mem

# Source files
SRCS = main.cpp sim_mem.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Build rule
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Object file rule
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJS) $(TARGET)