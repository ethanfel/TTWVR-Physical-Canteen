# SPDX-License-Identifier: GPL-3.0-only
WINCXX ?= i686-w64-mingw32-g++
FLAGS := -std=c++17 -Wall -Wextra -Werror -O2
NAME := TTWVRPhysicalCanteen
.PHONY: all test package
all: build/$(NAME).dll build/$(NAME).esp
build:
	mkdir -p $@
build/$(NAME).dll: src/Plugin.cpp src/HandAnimation.h src/Scene.h src/Fingers.h src/Grip.h src/Sip.h | build
	$(WINCXX) $(FLAGS) -static -shared -s $< -o $@
build/$(NAME).esp: tools/item.py | build
	python3 $< $@
build/test-sip: tests/Sip.cpp src/Sip.h | build
	$(CXX) $(FLAGS) $< -o $@
build/test-grip: tests/Grip.cpp src/Fingers.h src/Grip.h src/Sip.h | build
	$(CXX) $(FLAGS) $< -o $@
test: build/test-sip build/test-grip
	./build/test-sip
	./build/test-grip
package: all test
	python3 tools/package.py
