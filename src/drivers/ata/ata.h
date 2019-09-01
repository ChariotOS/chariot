#ifndef __ATA_DRIVER_H__
#define __ATA_DRIVER_H__

#include <asm.h>
#include <dev/blk_dev.h>
#include <pci.h>
#include <types.h>

class byte_port {
 private:
  u16 m_port;

 public:
  inline byte_port(u16 port) : m_port(port) {}
  inline byte_port() : m_port(0) {}

  inline void out(u8 val) { ::outb(m_port, val); }

  inline u8 in(void) { return ::inb(m_port); }
};

class word_port {
 public:
  u16 m_port;
  inline word_port(u16 port) : m_port(port) {}
  inline word_port() : m_port(0) {}

  inline void out(u16 val) { ::outw(m_port, val); }

  inline u16 in(void) { return ::inw(m_port); }
};

namespace dev {
class ata : public dev::blk_dev {
 protected:
  word_port data_port;
  byte_port error_port;
  byte_port sector_count_port;
  byte_port lba_low_port;
  byte_port lba_mid_port;
  byte_port lba_high_port;
  byte_port device_port;
  byte_port command_port;
  byte_port control_port;

  u16 m_io_base;

  struct pdrt {
    u64 offset;
    u16 size{0};
    u16 end_of_table{0};
  };

  bool master;
  u16 sector_size;

  void select_device();

  // save the identify buffer in the struct
  u16* id_buf = nullptr;

  u8 wait(void);

  u64 n_sectors = 0;

  bool use_dma;
  // where DMA will take place
  void* m_dma_buffer = nullptr;

  pci::device* m_pci_dev;
  u32 bar4 = 0;
  u16 bmr_command;
  u16 bmr_status;
  u16 bmr_prdt;

  struct prdt_t {
    uint32_t buffer_phys;
    uint16_t transfer_size;
    uint16_t mark_end;
  } __attribute__((packed));

 public:
  ata(u16 portbase, bool master);
  ~ata();

  bool identify();

  virtual bool read_block(u32 sector, u8* data);
  virtual bool write_block(u32 sector, const u8* data);
  virtual size_t block_size(void);
  virtual ssize_t size(void); // how big the drive is in bytes

  bool read_block_dma(u32 sector, u8* data);
  bool write_block_dma(u32 sector, const u8* data);

  // flush the internal buffer on the disk
  bool flush();

  u64 sector_count(void);
};
};  // namespace dev

#endif
