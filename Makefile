CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -Isrc
BUILD_DIR := build
BIN := $(BUILD_DIR)/lattice-dtl
SOURCES := \
	src/ldtl_common.c \
	src/asset_registry.c \
	src/ledger.c \
	src/matrix.c \
	src/epoch_netting.c \
	src/withdrawals.c \
	src/reconciliation.c \
	src/policy.c \
	src/runtime.c \
	src/main.c

.PHONY: all build test clean

all: build

build: $(BIN)

$(BIN): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) -o $(BIN)

test: build
	node --test "tests/node/*.test.js"

clean:
	rm -rf $(BUILD_DIR)
