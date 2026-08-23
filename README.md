# my_kernel

This project is a workspace prepared for a simple x86 operating system kernel named `my_kernel` and its accompanying SDK applications. The project includes the kernel code, boot structure, a FAT16 disk image, and external applications that can be written to the disk.

## Contents

- `src/`: Kernel source code.
- `sdk/`: External application codes and a simple libc implementation.
- `Makefile`: Build, ISO creation, and disk content update rules.
- `grub.cfg`: GRUB bootloader configuration.
- `linker.ld`: Kernel linker script.
- `c.img`: FAT16 disk image (for applications).

## Features

- Compilation with 32-bit `gcc` and `nasm`
- Simple multitasking and windowing system built on top of it
- Copying user applications in ELF format to the FAT16 disk
- Execution support via QEMU

## Requirements

- `gcc` (with i686/32-bit support)
- `nasm`
- `grub-mkrescue`
- `mtools`
- `qemu-system-i386`

## Build

To build the project, navigate to the project root directory in your terminal and run:

```bash
make
```

This command compiles the kernel, creates the `myos.bin` file, prepares the bootable `myos.iso` image, and adds the applications to the `c.img` disk image.

## Running

To run it on QEMU:

```bash
make run
```

This command starts the operating system in the virtual machine using the `myos.iso` and `c.img` files.

## Cleaning

To remove the generated files:

```bash
make clean
```

## Notes

- SDK applications are compiled from the sources in the `sdk/` directory.
- The `grub-mkrescue` command is required during ISO creation.
