/* This file is very simple... */

#include "internal.h"
#include <ck/eventloop.h>


/**
 * the main function for the window server
 */
int main(int argc, char **argv) {
	// make an eventloop
  ck::eventloop loop;

	// construct the manager
	lumen::manager ctx;

	// run the loop
	loop.start();

  return 0;
}
