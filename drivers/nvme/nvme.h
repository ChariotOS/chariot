#pragma once


#include <pci.h>
#include <types.h>
#include <wait.h>
#include "nvme.bits.h"

#define NVME_VS(major, minor, tertiary) (((major) << 16) | ((minor) << 8) | (tertiary))


namespace nvme {

  /* A representation of the NVMe version */
  union version {
    uint32_t raw;
    struct {
      /** indicates the tertiary version */
      uint32_t ter : 8;
      /** indicates the minor version */
      uint32_t mnr : 8;
      /** indicates the major version */
      uint32_t mjr : 16;
    };
  };


  /* The raw NVMe Structure */
  struct mmio {
    uint64_t cap;
    nvme::version vs;
    uint32_t imask_set;
    uint32_t imask_clr;
    // Controller configuration
    uint32_t cc;
    uint32_t reserved1;
    // Controller status
    uint32_t csts;
    // NVM subsystem reset (optional)
    uint32_t nssr;
    // Admin queue attributes
    uint32_t aqa;
    // Admin submission queue base addres
    uint64_t asq;
    // Admin completion queue base address
    uint64_t acq;
    // Controller memory buffer location (optional)
    uint32_t cmbloc;
    // Controller memory buffer size (optional)
    uint32_t cmbsz;
    uint8_t reserved2[0xF00 - 0x40];
    // Command set specific
    uint8_t reserved3[0x1000 - 0x0F00];
  } __attribute__((packed));



  /* A kernel level representation of a controller */
  struct ctrl {
    /* Ideally, this would be abstracted and allow us to use fabrics, but I dont have that hardware so. */
    pci::device &dev;
    volatile nvme::mmio *mmio;
    wait::queue wq;


    // Queue count, including admin queue
    size_t queue_count;
    size_t max_queues;

    off_t queue_memory_physaddr;
    void *queue_memory;

    size_t doorbell_shift;

    ctrl(pci::device &dev);

    /* Nice wrappers around the CAP field */
    inline auto version(void) { return mmio->vs.raw; }
    inline auto timeout(void) { return NVME_CAP_TO_GET(mmio->cap); }
    inline auto mqes(void) { return NVME_CAP_MQES_GET(mmio->cap); }
    inline auto stride(void) { return NVME_CAP_DSTRD_GET(mmio->cap); }
    inline auto css(void) { return NVME_CAP_CSS_GET(mmio->cap); }
    inline auto mps_min(void) { return NVME_CAP_MPSMIN_GET(mmio->cap); }
    inline auto mps_max(void) { return NVME_CAP_MPSMAX_GET(mmio->cap); }
    inline auto subsystem_reset_control(void) { return NVME_CAP_NSSRS_GET(mmio->cap); }
  };



  // Scatter gather list
  struct sgl {
    char data[16];
  };

  // Physical region pointer
  struct prp {
    uintptr_t addr;
  };

  union data_ptr {
    // Scatter gather list
    nvme::sgl sgl1;

    // Physical region page
    nvme::prp prpp[2];
  };

  struct cmd_hdr {
    // Command dword 0
    uint32_t cdw0;

    // namespace ID
    uint32_t nsid;

    uint64_t reserved1;

    // Metadata pointer
    uint64_t mptr;

    // Data pointer
    nvme::data_ptr dptr;
  };


  struct cmd {
    nvme::cmd_hdr hdr;
    // cmd dwords 10 thru 15
    uint32_t cmd_dword_10[6];

#if 0
    // 5.11 Identify command
    static nvme::cmd create_identify(void *addr, uint8_t cns, uint8_t nsid);
    // Create a command that creates a submission queue
    static nvme::cmd create_sub_queue(void *addr, uint32_t size, uint16_t sqid, uint16_t cqid, uint8_t prio);
    // Create a command that creates a completion queue
    static nvme::cmd create_cmp_queue(void *addr, uint32_t size, uint16_t cqid, uint16_t intr);
    // Create a command that reads storage
    static nvme::cmd create_read(uint64_t lba, uint32_t count, uint8_t ns);
    // Create a command that writes storage
    static nvme::cmd create_write(uint64_t lba, uint32_t count, uint8_t ns, bool fua);
    // Create a command that does a trim
    static nvme::cmd create_trim(uint64_t lba, uint32_t count, uint8_t ns);
    // Create a flush command
    static nvme::cmd create_flush(uint8_t ns);
    // Create a command that sets the completion and submission queue counts
    static nvme::cmd create_setfeatures(uint16_t ncqr, uint16_t nsqr);
#endif
  };


  /* Completion Queue Entry */
  struct cmpl {
    // Command specific
    uint32_t cmp_dword[4];
  };


  template <typename T>
  struct queue {
   public:
    void init(T *entries, uint32_t count, uint32_t volatile *head_doorbell, uint32_t volatile *tail_doorbell,
              bool phase) {
      // Only makes sense to have one doorbell
      // Submission queues have tail doorbells
      // Completion queues have head doorbells
      assert((head_doorbell && !tail_doorbell) || (!head_doorbell && tail_doorbell));

      this->entries = entries;
      this->mask = count - 1;
      this->head_doorbell = head_doorbell;
      this->tail_doorbell = tail_doorbell;
      this->phase = phase;
    }


    /* Return a pointer to the queue buffer */
    T *data() { return entries; }


    uint32_t get_tail() const { return tail; }
    bool get_phase() const { return phase; }
    bool is_empty() const { return head == tail; }
    bool is_full() const { return next(tail) == head; }


    template <typename... Args>
    uint32_t enqueue(T &&item) {
      size_t index = tail;
      entries[tail] = move(item);
      phase ^= set_tail(next(tail));
      return index;
    }

    T &at_tail(size_t tail_offset, bool &ret_phase) {
      uint32_t index = (tail + tail_offset) & mask;
      ret_phase = phase ^ (index < tail);
      return entries[index];
    }

    // Returns the number of items ready to be dequeued
    uint32_t count() const { return (tail - head) & mask; }

    // Returns the number of items that may be enqueued
    uint32_t space() const { return (head - tail) & mask; }

    uint32_t enqueued(uint32_t count) {
      uint32_t index = (tail + count) & mask;
      phase ^= set_tail(index);
      return index;
    }

    T dequeue() {
      T item = entries[head];
      take(1);
      return item;
    }

    // Used when completions have been consumed
    void take(uint32_t count) {
      uint32_t index = (head + count) & mask;
      phase ^= set_head(index);
    }

    // Used when processing completions to free up submission queue entries
    void take_until(uint32_t new_head) { phase ^= set_head(new_head); }

    T const &at_head(uint32_t head_offset, bool &expect_phase) const {
      uint32_t index = (head + head_offset) & mask;
      expect_phase = phase ^ (index < head);
      return entries[(head + head_offset) & mask];
    }

    void reset() {
      while (!is_empty()) dequeue();
      head = 0;
      tail = 0;
    }

   private:
    uint32_t next(uint32_t index) const { return (index + 1) & mask; }

    // Returns 1 if the queue wrapped
    bool set_head(uint32_t new_head) {
      bool wrapped = new_head < head;

      head = new_head;

      if (head_doorbell) *head_doorbell = head;

      return wrapped;
    }

    // Returns 1 if the queue wrapped
    bool set_tail(uint32_t new_tail) {
      bool wrapped = new_tail < tail;

      tail = new_tail;

      if (tail_doorbell) *tail_doorbell = tail;

      return wrapped;
    }

    T *entries = nullptr;
    uint32_t volatile *head_doorbell = nullptr;
    uint32_t volatile *tail_doorbell = nullptr;
    uint32_t mask = 0;
    uint32_t head = 0;
    uint32_t tail = 0;
    bool phase = true;
  };



};  // namespace nvme
