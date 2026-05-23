NASM ?= nasm
CC ?= gcc
LD ?= ld

BUILD_DIR := build
KERNEL_ELF := $(BUILD_DIR)/kernel.elf

INC := -IInclude -I../Common/Include -IExternal/flanterm/src
CFLAGS ?= -m64 -mcmodel=kernel $(INC) -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -mno-red-zone -nostdlib -Wall -Wextra

C_SRCS := $(shell find Source ../Common/Source External/flanterm/src -type f -name '*.c')
ASM_SRCS := $(shell find Source -type f -name '*.asm')

C_OBJS := $(patsubst Source/%.c,$(BUILD_DIR)/Source/%.c.o,\
          $(patsubst ../Common/Source/%.c,$(BUILD_DIR)/Common/%.c.o,\
          $(patsubst External/flanterm/src/%.c,$(BUILD_DIR)/External/flanterm/%.c.o,\
          $(C_SRCS))))
ASM_OBJS := $(patsubst Source/%.asm,$(BUILD_DIR)/Source/%.asm.o,$(ASM_SRCS))
KERNEL_OBJS := $(C_OBJS) $(ASM_OBJS)
DEPS := $(KERNEL_OBJS:.o=.d)

.PHONY: all clean

all: $(KERNEL_ELF)

$(BUILD_DIR)/Source/%.c.o: Source/%.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/Common/%.c.o: ../Common/Source/%.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/External/flanterm/%.c.o: External/flanterm/src/%.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/Source/%.asm.o: Source/%.asm Makefile
	mkdir -p $(dir $@)
	$(NASM) -f elf64 -M -MP -MT "$@ $(@:.o=.d)" -MF $(@:.o=.d) $<
	$(NASM) -f elf64 $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS) Linker.ld
	$(LD) -m elf_x86_64 -nostdlib -T Linker.ld -o $@ $(KERNEL_OBJS)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

clean:
	rm -rf $(BUILD_DIR)
