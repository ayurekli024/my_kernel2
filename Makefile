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

# ÖNCE LIBC KÜTÜPHANESİNİ DERLE!
sdk/libc.o: sdk/libc.c
	$(CC) $(APP_CFLAGS) -c sdk/libc.c -o sdk/libc.o

shell.elf: sdk/shell.c sdk/libc.o
	$(CC) $(APP_CFLAGS) -c sdk/shell.c -o sdk/shell.o
	ld -m elf_i386 -T sdk/app.ld sdk/shell.o sdk/libc.o -o shell.elf

yilan.elf: sdk/yilan.c sdk/libc.o
	$(CC) $(APP_CFLAGS) -c sdk/yilan.c -o sdk/yilan.o
	ld -m elf_i386 -T sdk/app.ld sdk/yilan.o sdk/libc.o -o yilan.elf

okuyucu.elf: sdk/okuyucu.c sdk/libc.o
	$(CC) $(APP_CFLAGS) -c sdk/okuyucu.c -o sdk/okuyucu.o
	ld -m elf_i386 -T sdk/app.ld sdk/okuyucu.o sdk/libc.o -o okuyucu.elf

bomba.elf: sdk/bomba.c sdk/libc.o
	$(CC) $(APP_CFLAGS) -c sdk/bomba.c -o sdk/bomba.o
	ld -m elf_i386 -T sdk/app.ld sdk/bomba.o sdk/libc.o -o bomba.elf

kedi.elf: sdk/kedi.c sdk/libc.o
	$(CC) $(APP_CFLAGS) -c sdk/kedi.c -o sdk/kedi.o
	ld -m elf_i386 -T sdk/app.ld sdk/kedi.o sdk/libc.o -o kedi.elf

daktilo.elf: sdk/daktilo.c sdk/libc.o
	$(CC) $(APP_CFLAGS) -c sdk/daktilo.c -o sdk/daktilo.o
	ld -m elf_i386 -T sdk/app.ld sdk/daktilo.o sdk/libc.o -o daktilo.elf

istemci.elf: sdk/istemci.c sdk/libc.o
	$(CC) $(APP_CFLAGS) -c sdk/istemci.c -o sdk/istemci.o
	ld -m elf_i386 -T sdk/app.ld sdk/istemci.o sdk/libc.o -o istemci.elf

# 4. Aşama: Uygulamaları FAT16 Diske (c.img) Enjekte Etme
disk: yilan.elf okuyucu.elf bomba.elf kedi.elf daktilo.elf istemci.elf shell.elf
	mcopy -o -i c.img yilan.elf ::/YILAN.ELF
	mcopy -o -i c.img okuyucu.elf ::/OKUYUCU.ELF
	mcopy -o -i c.img bomba.elf ::/BOMBA.ELF
	mcopy -o -i c.img kedi.elf ::/KEDI.ELF
	mcopy -o -i c.img daktilo.elf ::/DAKTILO.ELF
	mcopy -o -i c.img istemci.elf ::/ISTEMCI.ELF
	mcopy -o -i c.img shell.elf ::/SHELL.ELF

# 5. Aşama: QEMU'yu başlat (Başlamadan önce ISO ve disk otomatik güncellenir)
run: $(ISO_TARGET) disk
	qemu-system-i386 -audiodev pa,id=snd0 -machine pcspk-audiodev=snd0 -cdrom myos.iso -netdev user,id=net0 -device rtl8139,netdev=net0 -drive file=c.img,format=raw -boot d

# Temizlik
clean:
	rm -f src/*.o sdk/*.o $(TARGET) $(ISO_TARGET) yilan.elf okuyucu.elf bomba.elf kedi.elf daktilo.elf istemci.elf shell.elf
	rm -rf isodir