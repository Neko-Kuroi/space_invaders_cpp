# Space Invaders - C++ 版（SDL2）
# 必要な開発パッケージ（Debian/Ubuntu）:
#   libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
#   （C版と同じ。コンパイラは g++ を使う）

CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer SDL2_ttf)
SDL_LIBS   := $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer SDL2_ttf)

TARGET = space_invaders_cpp
SRC = src/main.cpp src/game.cpp src/assets.cpp src/logic.cpp

all: $(TARGET)

$(TARGET): $(SRC) src/game.hpp src/assets.hpp src/logic.hpp
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -o $@ $(SRC) $(SDL_LIBS)

test: tests/test_logic
	./tests/test_logic

tests/test_logic: tests/test_logic.cpp src/logic.cpp src/logic.hpp
	$(CXX) $(CXXFLAGS) -Isrc -o $@ tests/test_logic.cpp src/logic.cpp

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) tests/test_logic

.PHONY: all test run clean
