# SPDX-License-Identifier: GPL-3.0-only
WINCXX ?= i686-w64-mingw32-g++
FLAGS := -std=c++17 -Wall -Wextra -Werror -O2
NAME := TTWVRPhysicalCanteen
.PHONY: all test package
all: build/$(NAME).dll build/$(NAME).esp
build:
	mkdir -p $@
build/$(NAME).dll: src/Plugin.cpp src/Scene.h src/Holster.h src/Grip.h src/Sip.h src/DrinkTiming.h | build
	$(WINCXX) $(FLAGS) -static -shared -s $< -o $@
build/$(NAME).esp: tools/item.py | build
	python3 $< $@
build/test-sip: tests/Sip.cpp src/Sip.h | build
	$(CXX) $(FLAGS) $< -o $@
build/test-grip: tests/Grip.cpp src/Grip.h src/Sip.h | build
	$(CXX) $(FLAGS) $< -o $@
build/test-holster: tests/Holster.cpp src/Holster.h | build
	$(CXX) $(FLAGS) $< -o $@
build/test-draw: tests/Draw.cpp src/Scene.h src/Grip.h src/Sip.h | build
	$(CXX) $(FLAGS) $< -o $@
build/test-drink-timing: tests/DrinkTiming.cpp src/DrinkTiming.h | build
	$(CXX) $(FLAGS) $< -o $@
test: build/test-sip build/test-grip build/test-holster build/test-draw build/test-drink-timing
	./build/test-drink-timing
	python3 -B tests/test_drink_records.py
	./build/test-sip
	./build/test-holster
	./build/test-draw
	./build/test-grip
	python3 -B tests/test_native_grip.py
package: all test
	python3 tools/package.py
