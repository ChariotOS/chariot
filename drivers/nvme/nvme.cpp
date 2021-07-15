#include <mem.h>
#include <module.h>
#include <pci.h>
#include <printk.h>
#include <ck/vec.h>
#include <wait.h>
#include "nvme.bits.h"

#include "nvme.h"



namespace nvme {

#if 0

  // Command submission queue admin opcodes
  enum struct admin_cmd_opcode : uint8_t {
    delete_sq = 0x00,
    create_sq = 0x01,
    get_log_page = 0x02,
    delete_cq = 0x04,
    create_cq = 0x05,
    identify = 0x06,
    abort = 0x08,
    set_features = 0x09,
    get_features = 0x0A,
    async_evnt_req = 0x0C,
    firmware_commit = 0x10,
    firmware_download = 0x11,
    ns_attach = 0x15,
    keep_alive = 0x18
  };

  // Command submission queue NVM opcodes
  enum struct cmd_opcode : uint8_t {
    flush = 0x00,
    write = 0x01,
    read = 0x02,
    write_uncorrectable = 0x4,
    compare = 0x05,
    write_zeros = 0x08,
    dataset_mgmt = 0x09,
    reservation_reg = 0x0D,
    reservation_rep = 0x0E,
    reservation_acq = 0x11,
    reservation_rel = 0x15
  };

  enum struct feat_id : uint8_t { num_queues = 0x07 };




#endif



};  // namespace nvme



static ck::vec<ck::unique_ptr<nvme::ctrl>> controllers;


void nvme_init(void) {
  pci::walk_devices([](pci::device *dev) {
    if (dev->class_id == PCI_CLASS_STORAGE && dev->subclass_id == PCI_SUBCLASS_NVME) {
      controllers.push(ck::make_unique<nvme::ctrl>(*dev));
    }
  });
  printk(KERN_INFO "That's all the NVMe nonsense!\n");
}

module_init("NVMe", nvme_init);




#if 0
// 5.11 Identify command
nvme::cmd nvme::cmd::create_identify(void *addr, uint8_t cns, uint8_t nsid) {
  nvme::cmd cmd{};
  cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(uint8_t(nvme::admin_cmd_opcode::identify));
  cmd.hdr.nsid = nsid;
  cmd.hdr.dptr.prpp[0].addr = (unsigned long)v2p(addr);
  assert(cmd.hdr.dptr.prpp[0].addr);
  cmd.cmd_dword_10[0] = NVME_CMD_IDENT_CDW10_CNTID_n(0) | NVME_CMD_IDENT_CDW10_CNS_n(cns);
  return cmd;
}

// Create a "create submission queue" command
nvme::cmd nvme::cmd::create_sub_queue(void *addr, uint32_t size, uint16_t sqid, uint16_t cqid, uint8_t prio) {
  nvme::cmd cmd{};
  cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(uint8_t(nvme::admin_cmd_opcode::create_sq));
  cmd.hdr.dptr.prpp[0].addr = (unsigned long)v2p(addr);
  assert(cmd.hdr.dptr.prpp[0].addr);
  cmd.cmd_dword_10[0] = NVME_CMD_CSQ_CDW10_QSIZE_n(size - 1) | NVME_CMD_CSQ_CDW10_QID_n(sqid);
  cmd.cmd_dword_10[1] = NVME_CMD_CSQ_CDW11_CQID_n(cqid) | NVME_CMD_CSQ_CDW11_QPRIO_n(prio) | NVME_CMD_CSQ_CDW11_PC_n(1);
  return cmd;
}

// Create a "create completion queue" command
nvme::cmd nvme::cmd::create_cmp_queue(void *addr, uint32_t size, uint16_t cqid, uint16_t intr) {
  nvme::cmd cmd{};
  cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(uint8_t(nvme::admin_cmd_opcode::create_cq));
  cmd.hdr.dptr.prpp[0].addr = (unsigned long)p2v(addr);
  assert(cmd.hdr.dptr.prpp[0].addr);
  cmd.cmd_dword_10[0] = NVME_CMD_CCQ_CDW10_QSIZE_n(size - 1) | NVME_CMD_CCQ_CDW10_QID_n(cqid);
  cmd.cmd_dword_10[1] = NVME_CMD_CCQ_CDW11_IV_n(intr) | NVME_CMD_CCQ_CDW11_IEN_n(1) | NVME_CMD_CCQ_CDW11_PC_n(1);
  return cmd;
}

// Create a read command
nvme::cmd nvme::cmd::create_read(uint64_t lba, uint32_t count, uint8_t ns) {
  nvme::cmd cmd{};
  cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(uint8_t(nvme::cmd_opcode::read));
  cmd.hdr.nsid = ns;
  cmd.cmd_dword_10[0] = NVME_CMD_READ_CDW10_SLBA_n(uint32_t(lba & 0xFFFFFFFF));
  cmd.cmd_dword_10[1] = NVME_CMD_READ_CDW11_SLBA_n(uint32_t(lba >> 32));
  cmd.cmd_dword_10[2] = NVME_CMD_READ_CDW12_NLB_n(count - 1);
  return cmd;
}

// Create a write command
nvme::cmd nvme::cmd::create_write(uint64_t lba, uint32_t count, uint8_t ns, bool fua) {
  nvme::cmd cmd{};
  cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(uint8_t(nvme::cmd_opcode::write));
  cmd.hdr.nsid = ns;
  cmd.cmd_dword_10[0] = NVME_CMD_WRITE_CDW10_SLBA_n(uint32_t(lba & 0xFFFFFFFF));
  cmd.cmd_dword_10[1] = NVME_CMD_WRITE_CDW11_SLBA_n(uint32_t(lba >> 32));
  cmd.cmd_dword_10[2] = NVME_CMD_WRITE_CDW12_FUA_n((uint8_t)fua) | NVME_CMD_WRITE_CDW12_NLB_n(count - 1);
  return cmd;
}

// Create a trim command
nvme::cmd nvme::cmd::create_trim(uint64_t lba, uint32_t count, uint8_t ns) {
  nvme::cmd cmd{};

  return cmd;
}

// Create a flush command
nvme::cmd nvme::cmd::create_flush(uint8_t ns) {
  nvme::cmd cmd{};
  cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(uint8_t(nvme::cmd_opcode::flush));
  cmd.hdr.nsid = ns;
  return cmd;
}

// Create a setfeatures command
nvme::cmd nvme::cmd::create_setfeatures(uint16_t ncqr, uint16_t nsqr) {
  nvme::cmd cmd{};
  cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(uint8_t(nvme::admin_cmd_opcode::set_features));
  cmd.cmd_dword_10[0] = NVME_CMD_SETFEAT_CDW10_FID_n(uint8_t(nvme::feat_id::num_queues));
  cmd.cmd_dword_10[1] = NVME_CMD_SETFEAT_NQ_CDW11_NCQR_n(ncqr - 1) | NVME_CMD_SETFEAT_NQ_CDW11_NSQR_n(nsqr - 1);
  return cmd;
}

#endif
