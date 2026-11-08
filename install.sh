#!/usr/bin/env bash
set -euo pipefail

if command -v apt >/dev/null 2>&1; then
    sudo apt update
    sudo apt install -y pre-commit cppcheck clang-format cpplint clang-tidy googletest libgtest-dev cmake
elif command -v dnf >/dev/null 2>&1; then
    # clang-format and clang-tidy live in clang-tools-extra;
    # cpplint is not packaged in Fedora, so it is installed through pipx
    sudo dnf install -y pre-commit cppcheck clang-tools-extra gtest-devel gmock-devel cmake gcc-c++ make pipx
    pipx install cpplint
    pipx ensurepath
else
    echo "Unsupported distribution: neither apt nor dnf found" >&2
    exit 1
fi

pre-commit install