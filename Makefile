CC = gcc
CFLAGS = -m32 -c -std=gnu99 -ffreestanding -O2 -Wall -Wextra
ASM = nasm
ASMFLAGS = -f elf32
LD = gcc
LDFLAGS = -m32 -T linker.ld -nostdlib

# src klasöründeki tüm .c ve .asm dosyalarını otomatik bul
C_SOURCES = $(wildcard src/*.c)
ASM_SOURCES = $(wildcard src/*.asm)

# .c ve .asm uzantılarını .o uzantısına çevir
OBJ = $(ASM_SOURCES:.asm=.o) $(C_SOURCES:.c=.o)

TARGET = myos.bin

all: $(TARGET)

# Her bir Assembly dosyasını derleme kuralı
%.o: %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

# Her bir C dosyasını derleme kuralı
%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJ)

run: $(TARGET)
	qemu-system-i386 -kernel $(TARGET)

clean:
	rm -f src/*.o $(TARGET)