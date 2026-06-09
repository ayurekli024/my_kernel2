CC = gcc
CFLAGS = -m32 -c -std=gnu99 -ffreestanding -O2 -Wall -Wextra
ASM = nasm
ASMFLAGS = -f elf32
LD = gcc
LDFLAGS = -m32 -T linker.ld -nostdlib

OBJ = src/boot.o src/kernel.o
TARGET = myos.bin

all: $(TARGET)

src/boot.o: src/boot.asm
	$(ASM) $(ASMFLAGS) src/boot.asm -o src/boot.o

src/kernel.o: src/kernel.c
	$(CC) $(CFLAGS) src/kernel.c -o src/kernel.o

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJ)

run: $(TARGET)
	qemu-system-i386 -kernel $(TARGET) -m 128

clean:
	rm -f src/*.o $(TARGET)