BUILD_DIR ?= build
ITESTS ?= ""
ETESTS ?= ""

.PHONY: all configure build test unit-test e2e clean check doc run

all: build

SOURCES = $(wildcard src/*.c)
OBJECTS = $(patsubst src/%.c,$(BUILD_DIR)/make/%.o,$(SOURCES))
DEPFILES = $(OBJECTS:.o=.d)
ROOT_DIR := $(CURDIR)
PACK_DIR_NAME := .
PACK_ARCHIVE := $(ROOT_DIR)/compiler.zip

configure: $(BUILD_DIR)/CMakeCache.txt

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	cmake -S . -B $(BUILD_DIR)

build: ifj26-compiler

ifj26-compiler: $(OBJECTS)
	gcc -o $@ $(OBJECTS)

$(BUILD_DIR)/make/%.o: src/%.c
	mkdir -p $(dir $@)
	gcc -Wall -Wextra -std=c11 -g -MMD -MP -MF $(@:.o=.d) -c $< -o $@

-include $(DEPFILES)

run: build
	./ifj26-compiler < prog.ifj26 > prg.code
	./ic26int prg.code < prg.in > prg.out

test: unit-test e2e

unit-test: build configure
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure -L $(ITESTS) -E $(ETESTS)

e2e: build
	IC26INT=./ic26int IFJ26_COMPILER=./ifj26-compiler ./tests/e2e/run_e2e.sh $(E2E_ARGS)

check: build
	pre-commit run --files $$(git ls-files -m)

doc:
	doxygen Doxyfile

pack:
	set -e; \
	tmp_dir="$$(mktemp -d)"; \
	trap 'if [ -e "$$tmp_dir" ]; then rm -rf "$$tmp_dir"; fi' EXIT; \
	\
	mkdir -p "$$tmp_dir/$(PACK_DIR_NAME)/src"; \
	cp -a "$(ROOT_DIR)/src/." "$$tmp_dir/$(PACK_DIR_NAME)/src"; \
	\
	cp -a $(ROOT_DIR)/*.md "$$tmp_dir/$(PACK_DIR_NAME)"; \
	cp $(ROOT_DIR)/Makefile "$$tmp_dir/$(PACK_DIR_NAME)"; \
	cp $(ROOT_DIR)/rozsireni "$$tmp_dir/$(PACK_DIR_NAME)"; \
	cp $(ROOT_DIR)/rozdeleni "$$tmp_dir/$(PACK_DIR_NAME)"; \
	cp $(ROOT_DIR)/specifikace.txt "$$tmp_dir/$(PACK_DIR_NAME)"; \
	cp $(ROOT_DIR)/*.pdf "$$tmp_dir/$(PACK_DIR_NAME)"; \
	\
	mkdir -p "$$tmp_dir/$(PACK_DIR_NAME)/tests"; \
	cp -a $(ROOT_DIR)/tests/. "$$tmp_dir/$(PACK_DIR_NAME)/tests"; \
	\
	if [ -e "$(PACK_DIR_NAME).zip" ]; then rm "$(PACK_DIR_NAME).zip"; fi; \
	cd "$$tmp_dir"; \
	if [ -e "$(PACK_ARCHIVE)" ]; then rm "$(PACK_ARCHIVE)"; fi; \
	zip -rq "$(PACK_ARCHIVE)" "$(PACK_DIR_NAME)"; \
	echo "Created archive: $(PACK_ARCHIVE)"

clean:
	rm -rf $(BUILD_DIR) ifj26-compiler docs
