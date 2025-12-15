CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Iinclude
LDFLAGS ?=

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)

CXXFLAGS += $(SDL_CFLAGS)
LDFLAGS += $(SDL_LIBS)

SRC := $(shell find src -name '*.cpp')
OBJ := $(patsubst src/%.cpp, build/%.o, $(SRC))
TARGET := build/terra_clone

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build
