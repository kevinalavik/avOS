CC ?= gcc
CP ?= cp
MKDIR ?= mkdir
MCOPY ?= mcopy
MMD ?= mmd
MDEL ?= mdel

BUILD_DIR := build
TARGET := $(BUILD_DIR)/Shell.elf
EXEC_NAME := Shell
INSTALL_IMAGE := $(BUILD_DIR)/$(EXEC_NAME)
SRC := Source/Main.c
PREFIX ?= /Applications
LIBRARY_DIR := ../../Libc

CFLAGS ?= -g -m64 -ffreestanding -fno-stack-protector -fpie -nostdlib -Wall -Wextra -I$(LIBRARY_DIR)/Include
LDFLAGS ?= -nostdlib -pie -Wl,-e,_start -Wl,--build-id=none
CRT_OBJ := $(LIBRARY_DIR)/build/crt0.o
LIBC_A := $(LIBRARY_DIR)/build/libc.a
OBJ := $(BUILD_DIR)/Main.o

.PHONY: all install clean

all: $(TARGET)

$(CRT_OBJ) $(LIBC_A):
	$(MAKE) -C $(LIBRARY_DIR)

$(OBJ): $(SRC) $(CRT_OBJ) $(LIBC_A)
	$(MKDIR) -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ) $(CRT_OBJ) $(LIBC_A)
	$(CC) $(LDFLAGS) $(CRT_OBJ) $(OBJ) $(LIBC_A) -o $@

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
