CC = gcc
# YENİ: Stack boundary komutu silindi, sadece SSE kapatıldı.
CFLAGS = -m32 -c -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx
ASM = nasm
ASMFLAGS = -f elf32
LD = gcc
LDFLAGS = -m32 -T linker.ld -nostdlib -no-pie

# src klasöründeki tüm .c ve .asm dosyalarını otomatik bul
C_SOURCES = $(wildcard src/*.c)
ASM_SOURCES = $(wildcard src/*.asm)

# .c ve .asm uzantılarını .o uzantısına çevir
OBJ = $(ASM_SOURCES:.asm=.o) $(C_SOURCES:.c=.o)

TARGET = myos.bin
ISO_TARGET = myos.iso

all: $(ISO_TARGET)

# Her bir Assembly dosyasını derleme kuralı
%.o: %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

# Her bir C dosyasını derleme kuralı
%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

# 1. Aşama: myos.bin dosyasını oluştur
$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJ)

# 2. Aşama: BIN dosyasını alıp önyüklenebilir bir ISO imajına çevir
$(ISO_TARGET): $(TARGET) grub.cfg
	mkdir -p isodir/boot/grub
	cp $(TARGET) isodir/boot/$(TARGET)
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_TARGET) isodir
	rm -rf isodir

# 3. Aşama: QEMU'yu CD-ROM modunda ISO ile başlat
run: myos.iso
	qemu-system-i386 -cdrom myos.iso -hda c.img

# Temizlik (ISO ve isodir temizliği eklendi)
clean:
	rm -f src/*.o $(TARGET) $(ISO_TARGET)
	rm -rf isodir