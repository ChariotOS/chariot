#pragma once

#include <fs/tty.h>

// pseudo terminal backing structure
struct pty : public TTYNode {
  /* Directional Pipes */
  fifo_buf in;
  fifo_buf out;

  virtual ~pty(void);
  virtual void write_in(char c) override;
  virtual void write_out(char c, bool block = true) override;
  virtual ssize_t read(fs::File &, char *buf, size_t sz) override { return -1; }
};
