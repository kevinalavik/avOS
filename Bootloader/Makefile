NASM ?= nasm
CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy

BUILD_DIR := build
STAGE2_SOURCE_DIR := Stage2/Source
STAGE2_INCLUDE_DIR := Stage2/Include
COMMON_SOURCE_DIR := ../Common/Source
COMMON_INCLUDE_DIR := ../Common/Include
LOG_LEVEL ?= info

BOOTSTRAP_SRC := Bootstrap/Source/Bootstrap.asm
BOOTSTRAP_BIN := $(BUILD_DIR)/bootstrap.bin
BOOTSTRAP_DEP := $(BUILD_DIR)/bootstrap.d

STAGE2_LINKER := Linker.ld
STAGE2_ELF := $(BUILD_DIR)/stage2.elf
STAGE2_RAW := $(BUILD_DIR)/stage2.raw
STAGE2_BIN := $(BUILD_DIR)/stage2.bin

STAGE2_C_SRCS := $(sort $(shell find $(STAGE2_SOURCE_DIR) -type f -name '*.c'))
STAGE2_ASM_SRCS := $(sort $(shell find $(STAGE2_SOURCE_DIR) -type f -name '*.asm'))
COMMON_C_SRCS := $(sort $(shell find $(COMMON_SOURCE_DIR) -type f -name '*.c'))
STAGE2_C_OBJS := $(STAGE2_C_SRCS:$(STAGE2_SOURCE_DIR)/%.c=$(BUILD_DIR)/Stage2/%.c.o)
STAGE2_ASM_OBJS := $(STAGE2_ASM_SRCS:$(STAGE2_SOURCE_DIR)/%.asm=$(BUILD_DIR)/Stage2/%.asm.o)
COMMON_C_OBJS := $(COMMON_C_SRCS:$(COMMON_SOURCE_DIR)/%.c=$(BUILD_DIR)/Common/%.c.o)
STAGE2_OBJS := $(STAGE2_ASM_OBJS) $(STAGE2_C_OBJS) $(COMMON_C_OBJS)
DEPS := $(BOOTSTRAP_DEP) $(STAGE2_OBJS:.o=.d)

LOG_LEVEL_VALUE_debug := LogLevelDebug
LOG_LEVEL_VALUE_info := LogLevelInfo
LOG_LEVEL_VALUE_ok := LogLevelOk
LOG_LEVEL_VALUE_warn := LogLevelWarn
LOG_LEVEL_VALUE_error := LogLevelError
LOG_LEVEL_VALUE_DEBUG := LogLevelDebug
LOG_LEVEL_VALUE_INFO := LogLevelInfo
LOG_LEVEL_VALUE_OK := LogLevelOk
LOG_LEVEL_VALUE_WARN := LogLevelWarn
LOG_LEVEL_VALUE_ERROR := LogLevelError
STAGE2_LOG_LEVEL := $(LOG_LEVEL_VALUE_$(LOG_LEVEL))

ifeq ($(STAGE2_LOG_LEVEL),)
$(error Unsupported LOG_LEVEL '$(LOG_LEVEL)'; use debug, info, ok, warn, or error)
endif

CFLAGS ?= -m32 -I$(COMMON_INCLUDE_DIR) -I$(STAGE2_INCLUDE_DIR) -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -nostdlib -Wall -Wextra
CFLAGS += -DLogLevelThreshold=$(STAGE2_LOG_LEVEL)

.PHONY: all clean

all: $(BOOTSTRAP_BIN) $(STAGE2_BIN)

$(BUILD_DIR):
	mkdir -p $@

$(BOOTSTRAP_BIN): $(BOOTSTRAP_SRC) Makefile | $(BUILD_DIR)
	$(NASM) -f bin -M -MP -MT "$@ $(BOOTSTRAP_DEP)" -MF $(BOOTSTRAP_DEP) $<
	$(NASM) -f bin $< -o $@
	@test "$$(wc -c < $@)" -eq 512 || { echo "$@ must be exactly 512 bytes"; exit 1; }

$(BUILD_DIR)/Stage2/%.asm.o: $(STAGE2_SOURCE_DIR)/%.asm Makefile | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(NASM) -f elf32 -M -MP -MT "$@ $(@:.o=.d)" -MF $(@:.o=.d) $<
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/Stage2/%.c.o: $(STAGE2_SOURCE_DIR)/%.c Makefile | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/Common/%.c.o: $(COMMON_SOURCE_DIR)/%.c Makefile | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(STAGE2_RAW): $(STAGE2_OBJS) $(STAGE2_LINKER)
	$(LD) -m elf_i386 -nostdlib -T $(STAGE2_LINKER) -o $(STAGE2_ELF) $(STAGE2_OBJS)
	$(OBJCOPY) -O binary $(STAGE2_ELF) $@

$(STAGE2_BIN): $(STAGE2_RAW)
	cp $< $@

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

clean:
	rm -rf $(BUILD_DIR)
