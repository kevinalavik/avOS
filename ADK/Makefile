CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
CP ?= cp
MKDIR ?= mkdir

BUILD_DIR := build
INCLUDE_DIR := Include
SRC_DIR := Source
LIBRARY_DIR := ../Libc

ADK_A := $(BUILD_DIR)/libadk.a

CFLAGS ?= -g -O2 -m64 -ffreestanding -fno-stack-protector -fpie -nostdlib -Wall -Wextra -I$(INCLUDE_DIR) -I$(LIBRARY_DIR)/Include

ADK_SRCS := $(shell find $(SRC_DIR) -name '*.c' | sort)
ADK_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ADK_SRCS))

.PHONY: all install clean

all: $(ADK_A)

$(BUILD_DIR):
	$(MKDIR) -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(ADK_A): $(ADK_OBJS)
	$(AR) rcs $@ $(ADK_OBJS)
	$(RANLIB) $@

install: all
	@if [ -n "$(DESTDIR)" ] && [ -n "$(PREFIX)" ] && ! printf '%s' "$(DESTDIR)" | grep -q '@@'; then \
		$(MKDIR) -p "$(DESTDIR)$(PREFIX)/include" "$(DESTDIR)$(PREFIX)/lib"; \
		cp -r "$(INCLUDE_DIR)/." "$(DESTDIR)$(PREFIX)/include/"; \
		$(CP) "$(ADK_A)" "$(DESTDIR)$(PREFIX)/lib/libadk.a"; \
	fi

clean:
	rm -rf $(BUILD_DIR)
