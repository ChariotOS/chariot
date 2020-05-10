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

// create a new desktop window
#define DESKTOP_CREATE_WIN 10


// Window IOCTL commands
#define WINDOW_WAIT 0 // get the next event, blocking until there is one
#define WINDOW_POLL 1 // get the next event, but don't block

struct desktop_window_event {
  int type;  // WIN_EV_*
  union {
    struct {};
  } payload;
};

#endif
