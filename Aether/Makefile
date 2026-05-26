CC ?= gcc
CP ?= cp
MKDIR ?= mkdir
MCOPY ?= mcopy
MMD ?= mmd
MDEL ?= mdel

BUILD_DIR := build
TARGET := $(BUILD_DIR)/Aether.elf
EXEC_NAME := Aether
INSTALL_IMAGE := $(BUILD_DIR)/$(EXEC_NAME)
SRC_DIR := Source
PREFIX ?= /System
LIBRARY_DIR := ../Libc
ADK_DIR := ../ADK

CFLAGS ?= -g -O2 -m64 -ffreestanding -fno-stack-protector -fpie -nostdlib -Wall -Wextra -I$(LIBRARY_DIR)/Include -I$(ADK_DIR)/Include -IInclude
LDFLAGS ?= -nostdlib -pie -Wl,-e,_start -Wl,--build-id=none
CRT_OBJ := $(LIBRARY_DIR)/build/crt0.o
LIBC_A := $(LIBRARY_DIR)/build/libc.a
ADK_A := $(ADK_DIR)/build/libadk.a
C_SRCS := $(wildcard $(SRC_DIR)/*.c)
C_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))

.PHONY: all install clean

all: $(TARGET)

$(CRT_OBJ) $(LIBC_A):
	$(MAKE) -C $(LIBRARY_DIR)

$(ADK_A):
	$(MAKE) -C $(ADK_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(CRT_OBJ) $(LIBC_A) $(ADK_A)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(C_OBJS) $(CRT_OBJ) $(LIBC_A) $(ADK_A)
	$(CC) $(LDFLAGS) $(CRT_OBJ) $(C_OBJS) -Wl,--start-group $(LIBC_A) $(ADK_A) -Wl,--end-group -o $@

$(INSTALL_IMAGE): $(TARGET)
	$(CP) "$(TARGET)" "$(INSTALL_IMAGE)"

install: $(INSTALL_IMAGE)
	@if [ -z "$(DESTDIR)" ]; then \
		printf '%s\n' "DESTDIR is required for install"; \
		exit 1; \
	fi
	@if printf '%s' "$(DESTDIR)" | grep -q '@@'; then \
		case "$(PREFIX)" in \
			::*) image_prefix="$(PREFIX)" ;; \
			/*) image_prefix="::$$(printf '%s' "$(PREFIX)" | sed 's#^/*##')" ;; \
			*) image_prefix="::$(PREFIX)" ;; \
		esac; \
		$(MMD) -i "$(DESTDIR)" "$$image_prefix" >/dev/null 2>&1 || true; \
		printf '%s\n' "install $(INSTALL_IMAGE) -> $$image_prefix/$(EXEC_NAME)"; \
		$(MDEL) -i "$(DESTDIR)" "$$image_prefix/$(EXEC_NAME)" >/dev/null 2>&1 || true; \
		$(MCOPY) -D o -i "$(DESTDIR)" "$(INSTALL_IMAGE)" "$$image_prefix/$(EXEC_NAME)"; \
	else \
		$(MKDIR) -p "$(DESTDIR)$(PREFIX)"; \
		$(CP) "$(INSTALL_IMAGE)" "$(DESTDIR)$(PREFIX)/$(EXEC_NAME)"; \
	fi

clean:
	rm -rf $(BUILD_DIR)
