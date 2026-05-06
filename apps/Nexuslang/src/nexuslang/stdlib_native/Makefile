# Makefile -- simple build for libNLPL.a (fallback when CMake is unavailable)
#
# Usage:
#   make           -- build libNLPL.a
#   make shared    -- build libNLPL.so
#   make clean     -- remove build artefacts
#   make install   -- install to $(PREFIX)/lib and $(PREFIX)/include
#
# Variables you can override on the command line:
#   CC        -- C compiler        (default: cc)
#   AR        -- archiver          (default: ar)
#   CFLAGS    -- extra C flags
#   PREFIX    -- install prefix    (default: /usr/local)
#   BUILD_DIR -- output directory  (default: _build)

CC        ?= cc
AR        ?= ar
PREFIX    ?= /usr/local
BUILD_DIR ?= _build

CFLAGS  += -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter
CFLAGS  += -Iinclude

SOURCES := \
    src/nxl_runtime.c    \
    src/nxl_string.c     \
    src/nxl_math.c       \
    src/nxl_collections.c\
    src/nxl_io.c

OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

STATIC_LIB := $(BUILD_DIR)/libNLPL.a
SHARED_LIB := $(BUILD_DIR)/libNLPL.so

.PHONY: all static shared clean install

all: static

# ---------------------------------------------------------------------------
# Static library
# ---------------------------------------------------------------------------

static: $(STATIC_LIB)

$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs $@ $^
	@echo "Built static library: $@"

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ---------------------------------------------------------------------------
# Shared library
# ---------------------------------------------------------------------------

shared: CFLAGS += -fPIC
shared: $(SHARED_LIB)

$(SHARED_LIB): $(OBJECTS)
	$(CC) -shared -Wl,-soname,libNLPL.so -o $@ $^ -lm
	@echo "Built shared library: $@"

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

install: $(STATIC_LIB)
	install -d $(PREFIX)/lib $(PREFIX)/include
	install -m 644 $(STATIC_LIB) $(PREFIX)/lib/
	install -m 644 include/nxl_runtime.h $(PREFIX)/include/
	@echo "Installed to $(PREFIX)"

# ---------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned build artefacts"
