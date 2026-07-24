# my_kernel

Bu proje, `my_kernel` adlı basit bir x86 işletim sistemi çekirdeği ve beraberindeki SDK uygulamaları için hazırlanmış bir çalışma alanıdır. Proje; çekirdek kodu, boot yapısı, FAT16 disk imajı ve diske yazılabilir harici uygulamaları içerir.

## İçerik

- `src/`: Çekirdek kaynak kodu.
- `sdk/`: Harici uygulama kodları ve basit libc uygulaması.
- `Makefile`: Derleme, ISO oluşturma ve disk içeriklerini güncelleme kuralları.
- `grub.cfg`: GRUB önyükleyici yapılandırması.
- `linker.ld`: Çekirdek bağlayıcı betiği.
- `c.img`: FAT16 disk imajı (uygulamalar için).

## Özellikler

- 32-bit `gcc` ve `nasm` ile derleme
- Üzerine inşa edilmiş basit multitasking ve pencere sistemi
- Kullanıcı uygulamalarını ELF formatında FAT16 diske kopyalama
- QEMU ile çalıştırma desteği

## Gereksinimler

- `gcc` (i686/32-bit destekli)
- `nasm`
- `grub-mkrescue`
- `mtools`
- `qemu-system-i386`

## Derleme

Projeyi derlemek için terminalde proje kök dizinine gidin ve:

```bash
make
```

Bu komut, çekirdeği derler, `myos.bin` dosyasını oluşturur, `myos.iso` önyüklenebilir ISO görüntüsünü hazırlar ve `c.img` disk imajına uygulamaları ekler.

## Çalıştırma

QEMU üzerinde çalıştırmak için:

```bash
make run
```

Bu komut `myos.iso` ve `c.img` dosyalarını kullanarak sanal makinede işletim sistemini başlatır.

## Temizlik

Üretilen dosyaları kaldırmak için:

```bash
make clean
```

## Notlar

- SDK uygulamaları `sdk/` dizinindeki kaynaklardan derlenir.
- ISO oluşturma sırasında `grub-mkrescue` komutuna ihtiyaç vardır.
- Proje, 32-bit özgürleştirilmiş (`-ffreestanding`, `-nostdlib`) bir ortam kullanır.

## Lisans

Bu proje kişisel bir çalışma örneğidir. Lisans bilgisi proje sahibine aittir.
