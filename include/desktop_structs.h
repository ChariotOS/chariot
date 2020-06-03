#pragma once

#ifndef __CHARIOT_DESKTOP_HH
#define __CHARIOT_DESKTOP_HH

/*
 * Userspace and kernel communication structures for the desktop driver.
 *
 * The way the desktop driver works is simple. You first open /dev/desktop,
 * then use ioctl to create file descriptors for each new window. You can then
 * control those windows using ioctl much like linux's kvm interface.
 *
 * for example
 * +-----------------------------------------------------------------------+
 * int main() {
 *    int desktop = open("/dev/desktop", O_RDONLY);
 *    // error checking...
 *
 *    struct desktop_winops winops;
 *    // fill out winops..
 *
 *    // create the window
 *    int my_window = ioctl(desktop, DESKTOP_CREATE_WIN, &winops);
 *
 *    // sit in select loop or what have you on each window's events
 *
 *
 * }
 * +-----------------------------------------------------------------------+
 */


// Desktop IOCTL commands
// start and stop the desktop
#define DESKTOP_STARTUP 0
#define DESKTOP_SHUTDOWN 1

// load a mouse bitmap in, utilizing the desktop_load_mouse struct.
// This is needed in order to limit the desktop to *just* composition, meaning
// we dont need to have a png or bmp decoder in the kernel. That work is done
// in userspace
#define DESKTOP_LOAD_MOUSE 2

struct desktop_load_mouse {
  int type;  // what kind of mouse is this?
  int width, height;
  int ox, oy;  // how to offset the bitmap in order to line up the click target
               // with the tip of the mouse
  // a userspace pointer to the RGBA pixels that represent the mouse of `type`.
  // This is copied into kernel space if it makes sense (width and height are
  // less than 256x256 or whatever)
  unsigned int *pixels;
};


// create a new desktop window
#define DESKTOP_CREATE_WINDOW 10


// Window IOCTL commands
#define WINDOW_WAIT 0  // get the next event, blocking until there is one
#define WINDOW_POLL 1  // get the next event, but don't block

// pass a new control block for window information DMA
#define WINDOW_SETUP 2


#define WIN_EV_ERROR -1  // error state

struct desktop_window_event {
  int type;  // WIN_EV_*
  union {
    struct {};
  } payload;
};


/**
 * Every window has a shared (DMA ish) structure that the kernel and client
 * know about. When a window is created, this must be located at the start of
 * some memory region created with mmap and is treated as a "source of truth"
 *
 */
struct window_control_block {
  // the last event, populated by the kernel on WINDOW_WAIT or
  // WINDOW_POLL if there is something to do.
  struct desktop_window_event last_event;

  // the width and height of the window
  unsigned int width, height;

  // the bitmap follows directly
  unsigned int bitmap[];
};

#endif
