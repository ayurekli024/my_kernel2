#ifndef PCI_H
#define PCI_H

unsigned int pci_read_config_dword(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset);
void pci_write_config_dword(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset, unsigned int value);
unsigned int pci_get_device(unsigned short vendor_id, unsigned short device_id, unsigned char* bus_out, unsigned char* slot_out);

#endif