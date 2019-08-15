# Memory management / map

The memory management for the kernel in nOS is quite simple. When booting, the
kernel puts itself into paging long mode by mapping the first 512 pages 1:1 using
4k pages. As soon as possible, the kernel sets up real mappings. Like linux, nOS
uses very high virtual addresses for physical mappings. Kernel virtual memory starts
at the virtual address 0xffff880000000000, and any physical page accesses must be
offset by that address.

Physical memory is mapped globally starting at that address using the largest page
size possible. If the system has 1gb pages, use those; If it doesnt use 2mb pages.
Once this memory is mapped, the kernel heap is setup immediately after the physical
region in high memory. The kernel stack is then allocated in that heap.

Switching to the new address space for code uses a helper function that takes in
the function to run, and the stack to run it in.
