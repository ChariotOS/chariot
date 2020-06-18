/* This file is very simple... */

#include <ck/eventloop.h>
#include "internal.h"


/**
 * the main function for the window server
 */
int main(int argc, char **argv) {
  // make an eventloop
  ck::eventloop loop;

  // construct the context
  lumen::context ctx;

  // run the loop
  loop.start();

  return 0;
}
