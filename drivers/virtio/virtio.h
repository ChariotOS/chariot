#pragma once

#include <pci.h>


typedef uint64_t le64;
typedef uint32_t le32;
typedef uint16_t le16;
typedef uint8_t le8;


/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT 1
/* This marks a buffer as write-only (otherwise read-only). */
#define VIRTQ_DESC_F_WRITE 2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT 4

/* The device uses this in used->flags to advise the driver: don’t kick me
 * when you add a buffer.  It’s unreliable, so it’s simply an
 * optimization. */
#define VIRTQ_USED_F_NO_NOTIFY 1

/* The driver uses this in avail->flags to advise the device: don’t
 * interrupt me when you consume a buffer.  It’s unreliable, so it’s
 * simply an optimization.  */
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1


/* Device-independent feature bits. */

/* Force the device to interrupt if a virtqueue becomes empty (legacy). */
#define VIRTIO_F_NOTIFY_ON_EMPTY 24

/* Arbitrary descriptor layouts (legacy). */
#define VIRTIO_F_ANY_LAYOUT 27

/* Support for indirect descriptors */
#define VIRTIO_F_INDIRECT_DESC 28

/* Support for avail_event and used_event fields */
#define VIRTIO_F_EVENT_IDX 29

/* Compliant with standard (non-legacy) interface. */
#define VIRTIO_F_VERSION_1 32



// PCI defines
#define MAX_VIRTQS 4
#define VIRTIO_MSI_NO_VECTOR 0xffff


// everything virto-related lives here
namespace vio {


  /* Virtqueue descriptors: 16 bytes.
   * These can chain together via "next". */
  struct virtq_desc {
    /* Address (guest-physical). */
    le64 addr;
    /* Length. */
    le32 len;
    /* The flags as indicated above. */
    le16 flags;
    /* We chain unused descriptors via this, too */
    le16 next;
  };

  struct virtq_avail {
    le16 flags;
    le16 idx;
    le16 ring[];
    /* Only if VIRTIO_F_EVENT_IDX: le16 used_event; */
  };

  /* le32 is used here for ids for padding reasons. */
  struct virtq_used_elem {
    /* Index of start of used descriptor chain. */
    le32 id;
    /* Total length of the descriptor chain which was written to. */
    le32 len;
  };

  struct virtq_used {
    le16 flags;
    le16 idx;
    struct virtq_used_elem ring[];
    /* Only if VIRTIO_F_EVENT_IDX: le16 avail_event; */
  };

  struct virtq {
    uint16_t qsz;

    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;


    inline le16 *used_event(void) { return (le16 *)&avail->ring[qsz]; }
    inline le16 *avail_event(void) { return (le16 *)&used->ring[qsz]; }
  };

};  // namespace vio
