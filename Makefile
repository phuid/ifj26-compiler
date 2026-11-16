BUILD_DIR ?= build
ITESTS ?= ""
ETESTS ?= ""

.PHONY: all configure build test clean check doc run

all: build

SOURCES = $(wildcard src/*.c)

configure:
	cmake -S . -B $(BUILD_DIR)

build: $(SOURCES)
	gcc -Wall -Wextra -std=c11 -g $(SOURCES) -o ifj26-compiler

run: build
	./ifj26-compiler > prg.code
	ic26int prg.code < prg.in > prg.out

test: build configure
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure -L $(ITESTS) -E $(ETESTS)

check: build
	pre-commit run --files $$(git ls-files -m)

doc:
	doxygen Doxyfile

clean:
	rm -rf $(BUILD_DIR) ifj26-compiler docs
