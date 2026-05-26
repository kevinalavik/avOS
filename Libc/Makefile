CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
CP ?= cp
MKDIR ?= mkdir

BUILD_DIR := build
INCLUDE_DIR := Include
SRC_DIR := Source

CRT_OBJ := $(BUILD_DIR)/crt0.o
LIBC_A := $(BUILD_DIR)/libc.a

CFLAGS ?= -g -O2 -m64 -ffreestanding -fno-stack-protector -fpie -nostdlib -Wall -Wextra -I$(INCLUDE_DIR)

CRT_SRC := $(SRC_DIR)/crt0.S
LIBC_SRCS := $(shell find $(SRC_DIR) -name '*.c' | sort)
LIBC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIBC_SRCS))

.PHONY: all install clean

all: $(CRT_OBJ) $(LIBC_A)

$(BUILD_DIR):
	$(MKDIR) -p $(BUILD_DIR)

$(CRT_OBJ): $(CRT_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBC_A): $(LIBC_OBJS)
	$(AR) rcs $@ $(LIBC_OBJS)
	$(RANLIB) $@

install: all
	@if [ -n "$(DESTDIR)" ] && [ -n "$(PREFIX)" ] && ! printf '%s' "$(DESTDIR)" | grep -q '@@'; then \
		$(MKDIR) -p "$(DESTDIR)$(PREFIX)/include" "$(DESTDIR)$(PREFIX)/lib"; \
		cp -r "$(INCLUDE_DIR)/." "$(DESTDIR)$(PREFIX)/include/"; \
		$(CP) "$(CRT_OBJ)" "$(DESTDIR)$(PREFIX)/lib/crt0.o"; \
		$(CP) "$(LIBC_A)" "$(DESTDIR)$(PREFIX)/lib/libc.a"; \
	fi

clean:
	rm -rf $(BUILD_DIR)
