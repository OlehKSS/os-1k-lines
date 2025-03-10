#!/bin/bash
set -xue

#QEMU file path
QEMU=qemu-system-riscv32

# new: Path to clang and compiler flags
CC=clang
CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32 -ffreestanding -nostdlib"

OBJCOPY=/usr/bin/llvm-objcopy

# Build the shell (application)
$CC $CFLAGS -Wl,-Tuser.ld -Wl,-Map=shell.map -o shell.elf shell.c user.c common.c
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary shell.elf shell.bin
$OBJCOPY -Ibinary -Oelf32-littleriscv shell.bin shell.bin.o

# Build the kernel
# -ffreestanding	Do not use the standard library of the host environment (your development environment).
# -nostdlib	Do not link the standard library.
# -Wl,-Tkernel.ld	Specify the linker script.
# -Wl,-Map=kernel.map	Output a map file (linker allocation result).
$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf \
    kernel.c common.c shell.bin.o

# Create our tar-based filesystem
# The parentheses (...) create a subshell so that cd doesn't affect in other parts of the script.
(cd disk && tar cf ../disk.tar --format=ustar *.txt)

# The kernel.elf can be disassembled with
# llvm-objdump -d kernel.elf
# You can also check the addresses of functions/variables using
# llvm-nm kernel.elf
# Convert address to a file location
# llvm-addr2line -e kernel.elf 8020015e

# Start QEMU
# -machine virt: Start a virt machine. You can check other supported machines with the -machine '?' option.
# -bios default: Use the default firmware (OpenSBI in this case).
# -nographic: Start QEMU without a GUI window.
# -serial mon:stdio: Connect QEMU's standard input/output to the virtual machine's serial port.
#  Specifying mon: allows switching to the QEMU monitor by pressing Ctrl+A then C.
# --no-reboot: If the virtual machine crashes, stop the emulator without rebooting (useful for debugging).
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot \
    -d unimp,guest_errors,int,cpu_reset -D qemu.log \
    -drive id=drive0,file=disk.tar,format=raw,if=none \
    -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
    -kernel kernel.elf # Load the kernel

# To get more information about the CPU registers
# - info registers

# Check memory mapping
# - info mem

# Display the contents of physical memory
# - xp /32b 0x80265000

# Check binary contents
# - hexdump -C shell.bin 
