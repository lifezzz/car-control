# Makefile - build the four wheel car simulator.
#
#   make          build the program  -> ./car      (car.exe on Windows)
#   make test     build, then run every regression case in tests/cases
#   make clean    delete the build products
#
# The program is plain C11 without any third party library, so any C compiler
# will do:  make CC=gcc  /  make CC=clang.  MSVC users can use CMake instead
# (CMakeLists.txt), because MSVC does not understand -MMD/-MP.

CC       ?= cc
CFLAGS   ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude
LDFLAGS  ?=

SRC_DIR  := src
OBJ_DIR  := build/obj
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)

# Name of the produced program.
ifeq ($(OS),Windows_NT)
BIN := car.exe
else
BIN := car
endif

PYTHON ?= python3

.PHONY: all test clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

test: all
	$(PYTHON) tests/run_tests.py --bin ./$(BIN)

clean:
	$(RM) -r $(BIN) build

-include $(DEPS)
