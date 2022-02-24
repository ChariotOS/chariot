#pragma once

#include <fifo_buf.h>
#include <fs.h>
#include <ck/ptr.h>
#include <sched.h>
#include <ck/string.h>
#include <termios.h>

/**
 */
struct TTYNode : public fs::DeviceNode {
  int index;

  struct termios tios;
  struct winsize size;

  /* Controlling process */
  long ct_proc;
  /* Foreground process (can be the same as controller) */
  long fg_proc;


  bool controlled = false;
  bool next_is_verbatim;
  /* Used to store the line in canonical mode */
  ck::string canonical_buf;

  /* locked by the external API (read/write ops and whatnot) */
  spinlock lock;



  TTYNode();
  virtual ~TTYNode(void);


  void reset();

  virtual void write_in(char c){};
  virtual void write_out(char c, bool block = true){};

  void handle_input(char c);
  void output(char c, bool block = true);
  inline void echo(char c) { output(c, false); }
  void dump_input_buffer(void);


  void erase_one(int erase);

  // ^fs::Node
  virtual ssize_t read(fs::File &, char *dst, size_t count) = 0;
  virtual ssize_t write(fs::File &, const char *, size_t) final;
  virtual bool is_tty(void) final { return true; }
  virtual int ioctl(fs::File &, unsigned int cmd, off_t arg);


  ck::string name(void);
};
