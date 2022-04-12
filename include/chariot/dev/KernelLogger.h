#pragma once

#include <list_head.h>

// implemented this class if the device can act as a target for kernel console
// logging. If any devices are registered, the arch's serial_send will not be
// invoked.
//
// This class is implemented in fs/console.cpp
class KernelLogger {
 public:
	 KernelLogger(void) {
		 ent.init();
	 }
  ~KernelLogger(void);
  virtual int putc(int c) = 0;

	void register_logger();
	void deregister_logger();

	// ask every kernel logger to putc
	static void dispatch(int c);

 private:
	struct list_head ent;
};
