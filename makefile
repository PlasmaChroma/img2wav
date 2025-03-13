# Compiler
CXX = g++
# Compiler flags
CXXFLAGS = -Wall -std=c++17
# Executable name
TARGET = img2wav

# Source files
SRCS = main.cpp stb.cpp
# Object files
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(TARGET) $(OBJS)