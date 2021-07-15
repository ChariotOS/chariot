#pragma once

#include <fifo_buf.h>
#include <fs.h>
#include <ck/ptr.h>
#include <sched.h>
#include <ck/string.h>
#include <termios.h>

/**
 */
struct tty : public ck::refcounted<tty> {
  int index;

  struct termios tios;
  struct winsize size;

  /* Controlling process */
  pid_t ct_proc;
  /* Foreground process (can be the same as controller) */
  pid_t fg_proc;


  bool controlled = false;
  bool next_is_verbatim;
  /* Used to store the line in canonical mode */
  ck::string canonical_buf;

  /* locked by the external API (read/write ops and whatnot) */
  spinlock lock;



  tty();
  virtual ~tty(void);


  void reset();

  virtual void write_in(char c){};
  virtual void write_out(char c, bool block = true){};

  void handle_input(char c);
  void output(char c, bool block = true);
  inline void echo(char c) { output(c, false); }
  void dump_input_buffer(void);

  int ioctl(unsigned int cmd, off_t arg);

  void erase_one(int erase);

  ck::string name(void);
};
