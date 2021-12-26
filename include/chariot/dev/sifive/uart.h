#pragma once

#include <types.h>
#include <dev/driver.h>



#ifdef CONFIG_RISCV

namespace sifive {


  class Uart : public dev::SerialDevice {
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


		using dev::SerialDevice::SerialDevice;


    void put_char(char ch);
    int get_char(bool wait = true);

		virtual void init(void);

    // set the baud rate in the gp struct
    void setbrg(unsigned long clock, unsigned long baud);

    void handle_irq();

   private:
    Uart::Regs *regs = nullptr;
  };

	/*
  class UartDriver : public dev::DriverModule<sifive::Uart> {
   public:
    virtual ~UartDriver(void) {}
    dev::ProbeResult probe(ck::ref<hw::Device> dev);
  };
	*/



};  // namespace sifive

#endif
