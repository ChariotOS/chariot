#include <cpu.h>
#include <mem.h>
#include <phys.h>
#include <vec.h>
#include "nvme.h"

static int ilog2(int x) {
  /*
   * Find the leftmost 1. Use a method that is similar to
   * binary search.
   */
  int result = 0;
  result = (!!(x >> 16)) << 4;  // if > 16?
  // based on previous result, if > (result + 8)
  result = result + ((!!(x >> (result + 8))) << 3);
  result = result + ((!!(x >> (result + 4))) << 2);
  result = result + ((!!(x >> (result + 2))) << 1);
  result = result + (!!(x >> (result + 1)));
  return result;
}



nvme::ctrl::ctrl(pci::device &dev) : dev(dev) {
  dev.enable_bus_mastering();

  /* Grab the mmio region from the bar0 of the pci device */
  mmio = (volatile nvme::mmio *)p2v(dev.get_bar(0).addr);
  printk(KERN_INFO "Found NVMe Controller at version %d.%d.%d\n", mmio->vs.mjr, mmio->vs.mnr,
         mmio->vs.ter);
  printk(KERN_INFO "mqes:                      %d\n", mqes());
  printk(KERN_INFO "timeout:                   %d\n", timeout());
  printk(KERN_INFO "stride:                    %d\n", stride());
  printk(KERN_INFO "command sets supported:    %d\n", css());
  printk(KERN_INFO "Memory Page Size Minimum:  %d\n", mps_min());
  printk(KERN_INFO "Memory Page Size Maximum:  %d\n", mps_max());
  printk(KERN_INFO "Subsystem Reset Control:   %d\n", subsystem_reset_control());
  printk(KERN_INFO "\n");



  return;
  /* idk, honestly */
  size_t requested_queue_count = cpu::nproc() + 1;


  // 7.6.1 Initialization

  // Enable bus master DMA, enable MMIO, disable port I/O
  dev.adjust_ctrl_bits(PCI_CMD_BME | PCI_CMD_MSE, PCI_CMD_IOSE);

  // Disable the controller
  if (mmio->cc & NVME_CC_EN) mmio->cc &= ~NVME_CC_EN;

  // 7.6.1 2) Wait for the controller to indicate that any previous
  // reset is complete
  while ((mmio->csts & NVME_CSTS_RDY) != 0)
    arch_relax();

  // Attempt to use 64KB/16KB submission/completion queue sizes
  size_t queue_slots = 1024;
  assert(queue_slots <= 4096);
  size_t max_queue_slots = NVME_CAP_MQES_GET(mmio->cap) + 1;

  if (queue_slots > max_queue_slots) queue_slots = max_queue_slots;

  // Size of one queue, in bytes
  size_t queue_bytes = queue_slots * sizeof(nvme::cmd) + queue_slots * sizeof(nvme::cmpl);

  queue_count = 1;
  queue_bytes *= requested_queue_count;


  queue_memory_physaddr = (off_t)phys::alloc(round_up(queue_bytes, PGSIZE) / PGSIZE);
  queue_memory = p2v(queue_memory_physaddr);

  // 7.6.1 3) Configure the Admin queue
  mmio->aqa = NVME_AQA_ACQS_n(queue_slots) | NVME_AQA_ASQS_n(queue_slots);


  // Submission queue address
  mmio->asq = queue_memory_physaddr;


  // 3.1.10 The vector for the admin queues is always 0
  // Completion queue address
  mmio->acq = queue_memory_physaddr + (sizeof(nvme::cmd) * queue_slots * requested_queue_count);


  // 7.6.1 4) The controller settings should be configured
  uint32_t cc = NVME_CC_IOCQES_n(ilog2(sizeof(nvme::cmpl))) |
                NVME_CC_IOSQES_n(ilog2(sizeof(nvme::cmd))) | NVME_CC_MPS_n(0) | NVME_CC_CCS_n(0);


  // Try to enable weighted round robin with urgent
  if (NVME_CAP_AMS_GET(mmio->cap) & 1) cc |= NVME_CC_AMS_n(1);
  mmio->cc = cc;


  // Set enable with a separate write
  cc |= NVME_CC_EN;
  mmio->cc = cc;

  // 7.6.1 4) Wait for ready
  uint32_t ctrl_status;
  while (!((ctrl_status = mmio->csts) & (NVME_CSTS_RDY | NVME_CSTS_CFS)))
    arch_relax();


  if (ctrl_status & NVME_CSTS_CFS) {
    // Controller fatal status
    printk("Controller fatal status!\n");
    return;
  }

  // Read the doorbell stride
  doorbell_shift = NVME_CAP_DSTRD_GET(mmio->cap) + 1;

  // 7.4 Initialize queues

  // Initialize just the admin queue until we are sure about the
  // number of supported queues

  // nvme::cmd *sub_queue_ptr = (nvme::cmd *)queue_memory;
  // nvme::cmpl *cmp_queue_ptr = (nvme::cmpl *)(sub_queue_ptr + requested_queue_count *
  // queue_slots);


  // queues.reset(new (ext::nothrow) nvme_queue_state_t[requested_queue_count]);
}
