# ---- Toolchain ----
CC      := i686-elf-gcc
AS      := nasm
QEMU    := qemu-system-i386

# ---- Flags ----
CFLAGS  := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -Iinclude -Ikernel -Imm
ASFLAGS := -f elf32
LDFLAGS := -T linker.ld -ffreestanding -O2 -nostdlib

# ---- Sources ----
C_SRCS  := $(wildcard kernel/*.c) $(wildcard mm/*.c)
ASM_SRCS:= $(wildcard boot/*.asm) $(wildcard kernel/*.asm)

C_OBJS  := $(patsubst %.c,build/%.o,$(C_SRCS))
ASM_OBJS:= $(patsubst %.asm,build/%.o,$(ASM_SRCS))
OBJS    := $(ASM_OBJS) $(C_OBJS)

KERNEL  := build/kernel.bin
ISO     := build/os.iso

EXPECT  ?= HEAP_OK

.PHONY: all run test iso clean

all: $(KERNEL)

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -lgcc -o $@

run: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -serial stdio -display none -no-reboot

test: $(KERNEL)
	./scripts/test.sh $(EXPECT)

iso: $(KERNEL)
	@mkdir -p build/isodir/boot/grub
	cp $(KERNEL) build/isodir/boot/kernel.bin
	cp grub/grub.cfg build/isodir/boot/grub/grub.cfg
	i686-elf-grub-mkrescue -o $(ISO) build/isodir

clean:
	rm -rf build
