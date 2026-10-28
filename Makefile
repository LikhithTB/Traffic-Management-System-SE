# Traffic Management System (Simulation) - build file
# Usage:  make        build ./tms
#         make run    build and run
#         make clean  remove build output
CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2 -Isrc
SRCS     := $(wildcard src/*.cpp) $(wildcard src/*/*.cpp)
TARGET   := tms

all: $(TARGET)

$(TARGET): $(SRCS) $(wildcard src/*.h) $(wildcard src/*/*.h)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

run: $(TARGET)
	./$(TARGET)

clean:
	-$(RM) $(TARGET) $(TARGET).exe

.PHONY: all run clean
