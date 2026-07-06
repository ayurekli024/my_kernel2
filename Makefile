CC = gcc
# YENİ: Stack boundary komutu silindi, sadece SSE kapatıldı.
CFLAGS = -m32 -c -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic -mno-sse -mno-sse2 -mno-mmx
ASM = nasm
ASMFLAGS = -f elf32
LD = gcc
LDFLAGS = -m32 -T linker.ld -nostdlib -no-pie

# Harici uygulamalar için özel derleme bayrakları
APP_CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-sse -mno-sse2 -mno-mmx

# src klasöründeki tüm .c ve .asm dosyalarını otomatik bul
C_SOURCES = $(wildcard src/*.c)
ASM_SOURCES = $(wildcard src/*.asm)

# .c ve .asm uzantılarını .o uzantısına çevir
OBJ = $(ASM_SOURCES:.asm=.o) $(C_SOURCES:.c=.o)

TARGET = myos.bin
ISO_TARGET = myos.iso

all: $(ISO_TARGET) disk

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

# 3. Aşama: Harici Uygulamaları (SDK) Derleme
yilan.bin: sdk/yilan.c
	$(CC) $(APP_CFLAGS) -c sdk/yilan.c -o sdk/yilan.o
	ld -m elf_i386 -T sdk/app.ld sdk/yilan.o -o yilan.bin

okuyucu.bin: sdk/okuyucu.c
	$(CC) $(APP_CFLAGS) -c sdk/okuyucu.c -o sdk/okuyucu.o
	ld -m elf_i386 -T sdk/app.ld sdk/okuyucu.o -o okuyucu.bin
bomba.bin: sdk/bomba.c
	$(CC) $(APP_CFLAGS) -c sdk/bomba.c -o sdk/bomba.o
	ld -m elf_i386 -T sdk/app.ld sdk/bomba.o -o bomba.bin

# 4. Aşama: Uygulamaları FAT16 Diske (c.img) Enjekte Etme
disk: yilan.bin okuyucu.bin bomba.bin
	mcopy -o -i c.img yilan.bin ::/YILAN.BIN
	mcopy -o -i c.img okuyucu.bin ::/OKUYUCU.BIN
	mcopy -o -i c.img bomba.bin ::/BOMBA.BIN

# 5. Aşama: QEMU'yu başlat (Başlamadan önce ISO ve disk otomatik güncellenir)
run: $(ISO_TARGET) disk
	qemu-system-i386 -audiodev pa,id=snd0 -machine pcspk-audiodev=snd0 -cdrom myos.iso -drive file=c.img,format=raw -boot d

# Temizlik
clean:
	rm -f src/*.o sdk/*.o $(TARGET) $(ISO_TARGET) yilan.bin okuyucu.bin bomba.bin
	rm -rf isodir