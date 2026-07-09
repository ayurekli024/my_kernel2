#include "pci.h"
#include "io.h"

// PCI Donanımlarından Veri Okuma
unsigned int pci_read_config_dword(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset) {
    unsigned int address = (unsigned int)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

// PCI Donanımlarına Veri Yazma (Bus Mastering vb. ayarlar için)
void pci_write_config_dword(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset, unsigned int value) {
    unsigned int address = (unsigned int)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

// Anakarttaki 256 veriyolunu ve 32 slotu tarayıp hedef cihazı bulan dedektif fonksiyonumuz
unsigned int pci_get_device(unsigned short target_vendor, unsigned short target_device, unsigned char* bus_out, unsigned char* slot_out) {
    for (unsigned int bus = 0; bus < 256; bus++) {
        for (unsigned int slot = 0; slot < 32; slot++) {
            unsigned int vendor_device = pci_read_config_dword(bus, slot, 0, 0);
            if (vendor_device == 0xFFFFFFFF) continue; // Bu slot boş
            
            unsigned short current_vendor = vendor_device & 0xFFFF;
            unsigned short current_device = (vendor_device >> 16) & 0xFFFF;
            
            if (current_vendor == target_vendor && current_device == target_device) {
                *bus_out = bus;
                *slot_out = slot;
                return 1; // Cihaz Bulundu!
            }
        }
    }
    return 0; // Cihaz Bulunamadı
}