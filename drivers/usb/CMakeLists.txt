add_kernel_module(
	NAME usb
	NEEDS XHCI # for now!
	AUTOGLOB ./
)