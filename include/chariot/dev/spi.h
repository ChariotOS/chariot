#pragma once

#include <dev/driver.h>

namespace spi {

  struct Transfer {};

  class Device : public dev::Device {};

  class Controller : public dev::Device {
   public:
    i16 bus_num;
    /* chipselects will be integral to many controllers; some others
     * might use board-specific GPIOs.
     */
    u16 num_chipselect;
    /* some SPI controllers pose alignment requirements on DMAable
     * buffers; let protocol drivers know about these requirements.
     */
    u16 dma_alignment;

    /* spi_device.mode flags understood by this controller driver */
    u32 mode_bits;

    /* spi_device.mode flags override flags for this controller */
    u32 buswidth_override_bits;

    /* bitmask of supported bits_per_word for transfers */
    u32 bits_per_word_mask;

    /* limits on transfer speed */
    u32 min_speed_hz;
    u32 max_speed_hz;

    /* other constraints relevant to this driver */
    u16 flags;
#define SPI_CONTROLLER_HALF_DUPLEX BIT(0) /* can't do full duplex */
#define SPI_CONTROLLER_NO_RX BIT(1)       /* can't do buffer read */
#define SPI_CONTROLLER_NO_TX BIT(2)       /* can't do buffer write */
#define SPI_CONTROLLER_MUST_RX BIT(3)     /* requires rx */
#define SPI_CONTROLLER_MUST_TX BIT(4)     /* requires tx */

#define SPI_MASTER_GPIO_SS BIT(5) /* GPIO CS must select slave */

    /* flag indicating this is an SPI slave controller */
    bool slave;


    /*
     * on some hardware transfer / message size may be constrained
     * the limit may depend on device transfer settings
     */
    virtual size_t *max_transfer_size(spi::Device *spi) = 0;
    virtual size_t *max_message_size(spi::Device *spi) = 0;
  };

};  // namespace spi
