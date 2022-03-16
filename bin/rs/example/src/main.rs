#![no_std]
#![no_main]
#![feature(lang_items)]

// extern crate libc;

#[no_mangle]
pub extern "C" fn main(_argc: isize, _argv: *const *const u8) -> isize {
    0
}


use core::panic::PanicInfo;

#[lang = "panic_impl"]
fn panic_impl(_info: &PanicInfo) -> ! { loop {} }
#[lang = "eh_personality"]
fn eh_personality() {}
#[lang = "eh_catch_typeinfo"]
static EH_CATCH_TYPEINFO: u8 = 0;

