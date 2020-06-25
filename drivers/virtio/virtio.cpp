#include "virtio.h"
#include <module.h>
#include <util.h>

void virtio_init(void) {
	return;
  pci::walk_devices([](pci::device *d) {
    //
    if (d->vendor_id == 0x1af4) {



      auto &cfg = d->cfg;
      hexdump(&cfg, sizeof(cfg), true);

      debug("Virtio Device Found. Subsys: %04x.\n", cfg.dev_cfg.subsys_id);
      switch (cfg.dev_cfg.subsys_id) {
        case 1:
          debug("Net Device\n");
          // vdev->type = VIRTIO_PCI_NET;
          break;
        case 2:
          debug("Block Device\n");
          // vdev->type = VIRTIO_PCI_BLOCK;
          break;
        case 3:
          debug("Console Device\n");
          // vdev->type = VIRTIO_PCI_CONSOLE;
          break;
        case 4:
          debug("Entropy Device\n");
          // vdev->type = VIRTIO_PCI_ENTROPY;
          break;
        case 5:
          debug("Balloon Device\n");
          // vdev->type = VIRTIO_PCI_BALLOON;
          break;
        case 6:
          debug("IOMemory Device\n");
          // vdev->type = VIRTIO_PCI_IOMEM;
          break;
        case 7:
          debug("rpmsg Device\n");
          // vdev->type = VIRTIO_PCI_RPMSG;
          break;
        case 8:
          debug("SCSI Host Device\n");
          // vdev->type = VIRTIO_PCI_SCSI_HOST;
          break;
        case 9:
          debug("9P Transport Device\n");
          // vdev->type = VIRTIO_PCI_9P;
          break;
        case 10:
          debug("WIFI Device\n");
          // vdev->type = VIRTIO_PCI_WIFI;
          break;
        case 11:
          debug("rproc serial Device\n");
          // vdev->type = VIRTIO_PCI_RPROC_SERIAL;
          break;
        case 12:
          debug("CAIF Device\n");
          // vdev->type = VIRTIO_PCI_CAIF;
          break;
        case 13:
          debug("Fancier Balloon Device\n");
          // vdev->type = VIRTIO_PCI_FANCIER_BALLOON;
          break;
        case 16:
          debug("GPU Device\n");
          // vdev->type = VIRTIO_PCI_GPU;
          break;
        case 17:
          debug("Timer Device\n");
          // vdev->type = VIRTIO_PCI_TIMER;
          break;
        case 18:
          debug("Input Device\n");
          // vdev->type = VIRTIO_PCI_INPUT;
          break;
        default:
          debug("Unknown Device\n");
          // vdev->type = VIRTIO_PCI_UNKNOWN;
          break;
      }



      // struct virtio_pci_dev *vdev;
    }
  });
}

module_init("virtio", virtio_init);
