CC ?= gcc
CP ?= cp
MKDIR ?= mkdir
MCOPY ?= mcopy
MMD ?= mmd
MDEL ?= mdel
GIT ?= git
RM ?= rm -f

BUILD_DIR := build
TARGET := $(BUILD_DIR)/Doom.elf
EXEC_NAME := Doom
INSTALL_IMAGE := $(BUILD_DIR)/$(EXEC_NAME)
BUNDLE_NAME := Doom.app
INFO_FILE := Info
ICON_FILE := Icon.tga
SRC_DIR := Source
INCLUDE_DIR := Include
PREFIX ?= /Applications
AV_ROOT := $(abspath ../..)
LIBRARY_DIR := ../../Libc
ADK_DIR := ../../ADK

DOOM_REPO ?= https://github.com/ozkl/doomgeneric.git
DOOM_CHECKOUT := $(BUILD_DIR)/doomgeneric
DOOM_SRC_DIR := $(DOOM_CHECKOUT)/doomgeneric
PORT_STAMP := $(BUILD_DIR)/.av-port-installed
WAD_FILE ?= doom1.wad

.PHONY: all prepare install clean distclean

all: $(TARGET)

prepare: $(PORT_STAMP)

$(DOOM_CHECKOUT)/.cloned:
	@$(MKDIR) -p $(BUILD_DIR)
	@if [ ! -d "$(DOOM_CHECKOUT)/.git" ]; then \
		rm -rf "$(DOOM_CHECKOUT)"; \
		$(GIT) clone --depth 1 "$(DOOM_REPO)" "$(DOOM_CHECKOUT)"; \
	fi
	@touch $@

$(PORT_STAMP): $(DOOM_CHECKOUT)/.cloned $(SRC_DIR)/doomgeneric_av.c $(SRC_DIR)/doom_av_compat.c $(SRC_DIR)/Makefile.av
	$(CP) "$(SRC_DIR)/doomgeneric_av.c" "$(DOOM_SRC_DIR)/doomgeneric_av.c"
	$(CP) "$(SRC_DIR)/doom_av_compat.c" "$(DOOM_SRC_DIR)/doom_av_compat.c"
	$(CP) "$(SRC_DIR)/Makefile.av" "$(DOOM_SRC_DIR)/Makefile.av"
	@touch $@

$(LIBRARY_DIR)/build/crt0.o $(LIBRARY_DIR)/build/libc.a:
	$(MAKE) -C $(LIBRARY_DIR)

$(ADK_DIR)/build/libadk.a:
	$(MAKE) -C $(ADK_DIR)

$(TARGET): $(PORT_STAMP) $(LIBRARY_DIR)/build/crt0.o $(LIBRARY_DIR)/build/libc.a $(ADK_DIR)/build/libadk.a
	@$(MKDIR) -p $(BUILD_DIR)
	$(MAKE) -C "$(DOOM_SRC_DIR)" -f Makefile.av \
		CC="$(CC)" \
		AV_ROOT="$(AV_ROOT)" \
		APP_DIR="$(abspath .)" \
		OUTPUT="$(abspath $(TARGET))"

$(INSTALL_IMAGE): $(TARGET)
	$(CP) "$(TARGET)" "$(INSTALL_IMAGE)"

install: $(INSTALL_IMAGE)
	@if [ -z "$(DESTDIR)" ]; then \
		printf '%s\n' "DESTDIR is required for install"; \
		exit 1; \
	fi
	@if [ ! -f "$(WAD_FILE)" ]; then \
		printf '%s\n' "warning: $(WAD_FILE) not found; Doom will need doom1.wad in the app bundle"; \
	fi
	@if printf '%s' "$(DESTDIR)" | grep -q '@@'; then \
		case "$(PREFIX)" in \
			::*) image_prefix="$(PREFIX)" ;; \
			/*) image_prefix="::$$(printf '%s' "$(PREFIX)" | sed 's#^/*##')" ;; \
			*) image_prefix="::$(PREFIX)" ;; \
		esac; \
		bundle_path="$$image_prefix/$(BUNDLE_NAME)"; \
		$(MMD) -i "$(DESTDIR)" "$$image_prefix" >/dev/null 2>&1 || true; \
		$(MMD) -i "$(DESTDIR)" "$$bundle_path" >/dev/null 2>&1 || true; \
		$(MDEL) -i "$(DESTDIR)" "$$bundle_path/$(EXEC_NAME)" >/dev/null 2>&1 || true; \
		$(MDEL) -i "$(DESTDIR)" "$$bundle_path/$(INFO_FILE)" >/dev/null 2>&1 || true; \
		$(MDEL) -i "$(DESTDIR)" "$$bundle_path/doom1.wad" >/dev/null 2>&1 || true; \
		$(MDEL) -i "$(DESTDIR)" "$$bundle_path/$(ICON_FILE)" >/dev/null 2>&1 || true; \
		$(MCOPY) -D o -i "$(DESTDIR)" "$(INSTALL_IMAGE)" "$$bundle_path/$(EXEC_NAME)"; \
		$(MCOPY) -D o -i "$(DESTDIR)" "$(INFO_FILE)" "$$bundle_path/$(INFO_FILE)"; \
		$(MCOPY) -D o -i "$(DESTDIR)" "$(ICON_FILE)" "$$bundle_path/$(ICON_FILE)"; \
		if [ -f "$(WAD_FILE)" ]; then $(MCOPY) -D o -i "$(DESTDIR)" "$(WAD_FILE)" "$$bundle_path/doom1.wad"; fi; \
	else \
		$(MKDIR) -p "$(DESTDIR)$(PREFIX)/$(BUNDLE_NAME)"; \
		$(CP) "$(INSTALL_IMAGE)" "$(DESTDIR)$(PREFIX)/$(BUNDLE_NAME)/$(EXEC_NAME)"; \
		$(CP) "$(INFO_FILE)" "$(DESTDIR)$(PREFIX)/$(BUNDLE_NAME)/$(INFO_FILE)"; \
		$(CP) "$(ICON_FILE)" "$(DESTDIR)$(PREFIX)/$(BUNDLE_NAME)/$(ICON_FILE)"; \
		if [ -f "$(WAD_FILE)" ]; then $(CP) "$(WAD_FILE)" "$(DESTDIR)$(PREFIX)/$(BUNDLE_NAME)/doom1.wad"; fi; \
	fi

clean:
	@if [ -f "$(DOOM_SRC_DIR)/Makefile.av" ]; then \
		$(MAKE) -C "$(DOOM_SRC_DIR)" -f Makefile.av clean \
			AV_ROOT="$(AV_ROOT)" APP_DIR="$(abspath .)" OUTPUT="$(abspath $(TARGET))" || true; \
	fi
	rm -rf $(BUILD_DIR)/Doom.elf $(BUILD_DIR)/Doom $(PORT_STAMP)

distclean:
	rm -rf $(BUILD_DIR)
