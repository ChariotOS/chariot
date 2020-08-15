#pragma once

#include <fifo_buf.h>
#include <fs.h>
#include <sched.h>
#include <string.h>
#include <ptr.h>
#include <termios.h>

struct tty : public refcounted<tty> {
  int index;

  struct termios tios;
  struct winsize size;

  /* Directional Pipes */
  fifo_buf in;
  fifo_buf out;

  /* Controlling process */
  pid_t ct_proc;
  /* Foreground process (can be the same as controller) */
  pid_t fg_proc;


	bool controlled = false;
  bool next_is_verbatim;
  /* Used to store the line in canonical mode */
  string canonical_buf;

  /* locked by the external API (read/write ops and whatnot) */
  spinlock lock;

	static ref<struct tty> create(void);


	~tty(void);


	void reset();

  void write_in(char c);
  void write_out(char c);
  void handle_input(char c);
  void output(char c);
  void dump_input_buffer(void);

  void erase_one(int erase);

  string name(void);
};
