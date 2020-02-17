#![no_std] // don't link the Rust standard library

use core::panic::PanicInfo;



extern "C" {
pub fn write(
        fd: i32,
        buf: *const u8,
        count: usize,
    ) -> isize;
}


#[no_mangle] // don't mangle the name of this function
pub extern "C" fn my_func() {
    let msg = "hello\n";

    unsafe {
        write(0, msg.as_ptr(), msg.len());
    }
}

/// This function is called on panic.
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}
