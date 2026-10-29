# Traffic Management System (Simulation) - build file
#   make          build ./tms           (g++ -std=c++17, warnings are errors)
#   make run      build and run
#   make test     build and run all unit tests
#   make clean    remove build output
CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2 -Isrc
SRCS     := $(wildcard src/*.cpp) $(wildcard src/*/*.cpp)
LIBSRCS  := $(filter-out src/main.cpp,$(SRCS))
TESTSRCS := $(wildcard tests/*.cpp)
HEADERS  := $(wildcard src/*.h) $(wildcard src/*/*.h) $(wildcard tests/*.h)
TARGET   := tms
TESTBIN  := tms_tests

# On Windows (cmd.exe) programs in the current folder run without "./"
ifeq ($(OS),Windows_NT)
RUN :=
else
RUN := ./
endif

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

run: $(TARGET)
	$(RUN)$(TARGET)

$(TESTBIN): $(LIBSRCS) $(TESTSRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -Itests -o $@ $(LIBSRCS) $(TESTSRCS)

test: $(TESTBIN)
	$(RUN)$(TESTBIN)

clean:
	-$(RM) $(TARGET) $(TARGET).exe $(TESTBIN) $(TESTBIN).exe

.PHONY: all run test clean
