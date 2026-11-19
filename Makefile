BUILD_DIR ?= build
ITESTS ?= ""
ETESTS ?= ""

.PHONY: all build test clean check doc run

all: build

SOURCES = $(wildcard src/*.c)
ROOT_DIR := $(CURDIR)
PACK_DIR_NAME := .
PACK_ARCHIVE := $(ROOT_DIR)/compiler.zip

configure: $(BUILD_DIR)/CMakeCache.txt

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	cmake -S . -B $(BUILD_DIR)

build: $(SOURCES)
	gcc -Wall -Wextra -std=c11 -g $(SOURCES) -o ifj26-compiler

run: build
	./ifj26-compiler < prog.ifj26 > prg.code
	./ic26int prg.code < prg.in > prg.out

test: build configure
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure -L $(ITESTS) -E $(ETESTS)

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
