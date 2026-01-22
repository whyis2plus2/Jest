# on windows, use git bash

CC := gcc
LD := gcc

CFLAGS += -std=c99 -pedantic-errors
CFLAGS += -Wall -Wextra -Wunused -Wformat=2
CFLAGS +=

LDFLAGS +=

SRC_DIR := ./test
OUT_DIR := ./build

SRC := $(wildcard $(SRC_DIR)/*.c)
SRC += $(wildcard $(SRC_DIR)/**/*.c)

OBJ := $(SRC:%.c=%.o)

# if the file extension isn't specified on windows, then the makefile
# will re-link the executable every time `make run` or `make build` is used
ifeq ($(OS),Windows_NT)
	TARGET := $(OUT_DIR)/out.exe
else
	TARGET := $(OUT_DIR)/out
endif

build: $(TARGET)

run: build
	$(TARGET) $(ARGS)
	
clean:
	rm -rf $(OBJ) $(OUT_DIR)

$(TARGET): $(OBJ)
	@mkdir -p $(OUT_DIR)
	$(LD) $^ $(LDFLAGS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $< $(CFLAGS) -c -o $@
