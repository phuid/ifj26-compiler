BUILD_DIR ?= build
ITESTS ?= ""
ETESTS ?= ""

.PHONY: all configure build test clean

all: build

SOURCES = $(wildcard src/*.c)

configure:
	cmake -S . -B $(BUILD_DIR)

build:
	gcc -Wall -Wextra -Werror -std=c11 -g $(SOURCES) -o ifj26-compiler

run: build
	./ifj26-compiler

test: build configure
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure -L $(ITESTS) -E $(ETESTS)

check: build
	pre-commit run --files $$(git ls-files -m)

clean:
	rm -rf $(BUILD_DIR) ifj26-compiler
