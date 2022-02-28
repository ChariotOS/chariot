#pragma once

#include <types.h>
#include <dev/driver.h>



#ifdef CONFIG_RISCV

namespace sifive {


  class Uart : public dev::CharDevice {
   public:
    struct Regs {
      //
      volatile uint32_t txfifo;
      volatile uint32_t rxfifo;
      volatile uint32_t txctrl;
      volatile uint32_t rxctrl;
      volatile uint32_t ie;
      volatile uint32_t ip;
      volatile uint32_t div;
    };


    using dev::CharDevice::CharDevice;
    virtual ~Uart(void) {}
    void put_char(char ch);
    int get_char(bool wait = true);

    void init(void) override;
    void irq(int num) override;

    // set the baud rate in the gp struct
    void setbrg(unsigned long clock, unsigned long baud);


   private:
    Uart::Regs *regs = nullptr;
  };

};  // namespace sifive

#endif
