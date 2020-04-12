#![feature(asm)]


extern "C" {

    pub fn write(fd: i32, buf: *const u8, size: usize) -> i32;
}

fn print(msg: &[u8]) -> i32 {
    unsafe {
        write(1, msg.as_ptr(), msg.len())
    }
}

fn main() {
    print(b"hello, friend\n");
}
