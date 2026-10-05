BUILD_DIR ?= build
ITESTS ?= ""
ETESTS ?= ""


.PHONY: all configure build test clean

all: build

SOURCES = $(wildcard src/*.c)

build: 
	gcc -Wall -Wextra -Werror -std=c11 -g $(SOURCES) -o ifj26-compiler

test: build configure
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure -L $(ITESTS) -E $(ETESTS)

clean:
	rm -rf $(BUILD_DIR)
