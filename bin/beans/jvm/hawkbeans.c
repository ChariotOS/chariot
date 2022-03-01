/* 
 * This file is part of the Hawkbeans JVM developed by
 * the HExSA Lab at Illinois Institute of Technology.
 *
 * Copyright (c) 2017, Kyle C. Hale <khale@cs.iit.edu>
 *
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the 
 * file "LICENSE.txt".
 */
#include <stdio.h>
#include <stdlib.h>
// #include <getopt.h>

#include <shell.h>
#include <hawkbeans.h>
#include <bc_interp.h>
#include <class.h>
#include <thread.h>
#include <stack.h>
#include <mm.h>
#include <gc.h>

#include <arch/x64-linux/bootstrap_loader.h>

jthread_t * cur_thread;

static void
version (void)
{
	printf("Hawkbeans Java Virtual Machine Version %s\n", HAWKBEANS_VERSION_STRING);
	printf("Kyle C. Hale (c) 2019, Illinois Institute of Technology\n");
	exit(EXIT_SUCCESS);
}


static void 
usage (const char * prog)
{
	fprintf(stderr, "Hawkbeans Java Virtual Machine (v%s)\n\n", HAWKBEANS_VERSION_STRING);
	fprintf(stderr, "%s [options] classfile\n\n", prog);
	fprintf(stderr, "Arguments:\n\n");
	fprintf(stderr, " %20.20s Print the version number and exit\n", "--version, -V");
	fprintf(stderr, " %20.20s Print this message\n", "--help, -h");
    fprintf(stderr, " %20.20s Start the Hawkbeans shell immediately\n", "--interactive, -i");
	fprintf(stderr, " %20.20s Set the heap size (in MB). Default is 1MB.\n", "--heap-size, -H");
	fprintf(stderr, " %20.20s Trace the Garbage Collector\n", "--trace-gc, -t");
	fprintf(stderr, " %20.20s GC collection interval in ms (default=%d)\n", "--gc-interval, -c", GC_DEFAULT_INTERVAL);
    fprintf(stderr, " %20.20s Run in interactive mode (shell)\n", "--interactive, -i");
	fprintf(stderr, "\n\n");
	exit(EXIT_SUCCESS);
}

#if 0
static struct option long_options[] = {
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"heap-size", required_argument, 0, 'H'},
	{"trace-gc", no_argument, 0, 't'},
	{"gc-interval", required_argument, 0, 'c'},
    {"interactive", no_argument, 0, 'i'},
	{0, 0, 0, 0}
};
#endif


static struct gopts {
	int heap_size_megs;
	const char * class_path;
	bool trace_gc;
	int gc_interval;
    bool interactive;
} glob_opts;


static obj_ref_t * 
create_argv_array (int argc, char ** argv) 
{
	int i;
	obj_ref_t * arr = array_alloc(T_REF, argc);
	native_obj_t * arr_obj = NULL;
	
	if (!arr) {
		HB_ERR("Could not create jargv array\n"); 
		return NULL;
	}

	arr_obj = (native_obj_t*)arr->heap_ptr;

	for (i = 0; i < argc; i++) {
		obj_ref_t * str_obj = string_object_alloc(argv[i]);
		if (!str_obj) {
			HB_ERR("Could not create string object for argv array\n");
			return NULL;
		}
		arr_obj->fields[i].obj = str_obj;
	}
	
	return arr;
}



static int optind = 1;

static void
parse_args (int argc, char ** argv)
{
	int c;

#if 0
	while (1) {
		int opt_idx = 0;
		c = getopt_long(argc, argv, "c:hiVH:t", long_options, &opt_idx);
		
		if (c == -1) {
			break;
		}

		switch (c) {
			case 0:
				break;
			case 'V':
				version();
				break;
			case 'h':
				usage(argv[0]);
				break;
			case 'c':
				glob_opts.gc_interval = atoi(optarg);
				break;
			case 'H': 
				glob_opts.heap_size_megs = atoi(optarg);
				break;
            case 'i':
                glob_opts.interactive = true;
                break;
			case 't':
				glob_opts.trace_gc = true;
			case '?':
				break;
			default:
				printf("Unknown option: %o.\n", c);
				break;
		}
	}

	if (optind >= argc) {
		usage(argv[0]);
	}
#endif
	
	// the rest are for String[] passed to java main
	glob_opts.class_path = argv[optind++];
}


int 
main (int argc, char ** argv) 
{
	java_class_t * cls      = NULL;
	obj_ref_t * obj         = NULL;
	obj_ref_t * jargv       = NULL;
	jthread_t * main_thread = NULL;
	int main_idx;

    // we first separate the JVM args (for us)
    // from the Java args (for the Java program)
	parse_args(argc, argv);

    // setup our heap using default sizes
	heap_init(glob_opts.heap_size_megs);

	// initialize the hashtable that stores loaded classes 
	hb_classmap_init();

    // load the class passed in at cmdline
    // Note that class loading goes as follows:
    // * load it
    // * add it to the class map
    // * prepare it (set static fields to their initial vals)
    // * init it (call the class initializer <clinit>, if it exists)
    // This sequence normally happens in hb_get_or_load_class(),
    // but here we have to do it explicitly, and interleaved
    // with other code because calling the class initializer
    // relies on other subsystems being set up properly
	cls = hb_load_class(glob_opts.class_path);

	if (!cls) {
		HB_ERR("Could not load base class (%s)\n", glob_opts.class_path);
		exit(EXIT_FAILURE);
	}

    // add our class to the class map
    hb_add_class(hb_get_class_name(cls), cls);

    // initialize static fields etc.
    hb_prep_class(cls);

    // allocate an object of type Class
	obj = object_alloc(cls);

    // find the main() method of our class, and wrap
    // a thread (our only thread) around it
	main_idx    = hb_get_method_idx("main", cls);
	main_thread = hb_create_thread(cls, "main");

	if (!main_thread) {
		HB_ERR("Could not create main thread\n");
		exit(EXIT_FAILURE);
	}

    // this will never change, currently
	cur_thread = main_thread;

    // This is our base stack frame. In just a sec,
    // you'll see me put our object instance (our
    // base object) on its local var array
    // before we execute any code
	hb_push_frame(main_thread, cls, main_idx);

    // set up our garbage collector
	gc_init(main_thread, obj, glob_opts.trace_gc, glob_opts.gc_interval);

    // get the Java args prepped
	jargv = create_argv_array(argc-optind, &argv[optind]);

	if (!jargv) {
		HB_ERR("Could not create Java argv array\n");
		exit(EXIT_FAILURE);
	}

    // Call our class's <clinit> (class initializer) function,
    // if it exists. Note that this actually will result in
    // bytecode instrs being interpreted! Even before main is
    // invoked. 
    hb_init_class(cls);

    // argv sits as first elm of the local variables stack
    main_thread->cur_frame->locals[0].obj = jargv;

    // Our argv array is just another object, but it 
    // wasn't allocated in the standard way (via GC), so we need to
    // explicitly add a ref to it so the GC can track it
	gc_insert_ref(jargv);
	
    // off to the races
    run_shell(main_thread, glob_opts.interactive);

	return 0;
}
