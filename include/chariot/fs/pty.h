#pragma once

#include <fs/tty.h>

// pseudo terminal backing structure
struct pty : public tty {
  /* Directional Pipes */
  fifo_buf in;
  fifo_buf out;

  virtual ~pty(void);
  virtual void write_in(char c) override;
  virtual void write_out(char c, bool block = true) override;
};
