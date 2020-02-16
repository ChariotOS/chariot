
// Don't link to std. We are std.
#![no_std]
#![feature(lang_items)]

pub mod rt;

#[no_mangle]
pub fn _rust_eh_personality() {}


use core::panic::PanicInfo;
/// This function is called on panic.
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[lang = "eh_personality"]
extern "C" fn eh_personality() {}
