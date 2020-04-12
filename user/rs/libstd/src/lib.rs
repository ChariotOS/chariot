#![crate_name="std"]
//#![feature(staged_api)]	// stability
#![feature(lang_items)]	// Allow definition of lang_items
#![feature(linkage)]	// Used for low-level runtime
#![feature(core_intrinsics)]
#![feature(const_fn)]
#![feature(box_syntax)]
#![feature(raw)]
#![feature(allocator_api)]
#![feature(allocator_internals)]
#![feature(test,custom_test_frameworks)]	// used for macro import
#![feature(asm,global_asm,concat_idents,format_args_nl,log_syntax)]
#![default_lib_allocator]
#![no_std]



// extern crate libc;


mod std {
	pub use core::{option, result};


	pub use core::fmt;
	pub use core::iter;
	pub use core::{mem, cmp, ops};
	pub use core::{str};
	pub use core::convert;
	// pub use ffi;
}


pub mod prelude {
	pub mod v1 {
		pub use core::marker::{/*Copy,*/Send,Sync,Sized};
		pub use core::ops::{Drop,Fn,FnMut,FnOnce};
		pub use core::mem::drop;
		// pub use alloc::boxed::Box;

		//pub use core::clone::Clone;
		//pub use core::cmp::{PartialEq, PartialOrd, Eq, Ord};
		pub use core::convert::{AsRef,AsMut,Into,From};
		//pub use core::default::Default;
		pub use core::iter::{Iterator,Extend,IntoIterator};
		pub use core::iter::{DoubleEndedIterator, ExactSizeIterator};
		pub use core::option::Option::{self,Some,None};
		pub use core::result::Result::{self,Ok,Err};


		// Macro imports?
		pub use core::prelude::v1::{
			Clone,
			Copy,
			Debug,
			Default,
			Eq,
			Hash,
			Ord,
			PartialEq,
			PartialOrd,
			RustcDecodable,
			RustcEncodable,
			// bench,
			global_allocator,
			test,
			// test_case,
			};
		pub use core::prelude::v1::{
			asm,
			assert,
			// cg,
			column,
			compile_error,
			concat,
			// concat_idents,
			env,
			file,
			format_args,
			// format_args_nl,
			// global_asm,
			include,
			include_bytes,
			include_str,
			line,
			// log_syntax,
			module_path,
			option_env,
			stringify,
			//trace_macros,
			};

    }
}




// These functions and traits are used by the compiler, but not
// for a bare-bones hello world. These are normally
// provided by libstd.

#[lang = "eh_personality"] extern fn eh_personality() {}
#[panic_handler]
pub extern fn rust_begin_unwind(_info: &::core::panic::PanicInfo) -> ! {
    loop {}
}



#[lang="termination"]
pub trait Termination
{
	fn report(self) -> i32;
}



impl Termination for ()
{
	fn report(self) -> i32 { 0 }
}
impl<T,E> Termination for Result<T,E>
where
	T: Termination//,
	//E: ::error::Error
{
	fn report(self) -> i32
	{
		match self
		{
		Ok(v) => v.report(),
		Err(_e) => 1,
		}
	}
}

#[no_mangle]
pub fn rs_libstd_start(
    main: fn(),
    _argc: isize,
    _argv: *const *const u8,
    _envp: *const *const u8
) -> ! {



    main();

    let buf = unsafe { &mut *(0 as *mut u8) };
    *buf = 0;

    loop {}
}


#[lang="start"]
pub fn lang_start<T: Termination+'static>(_main: fn()->T, _argc: isize, _argv: *const *const u8) -> isize {
    0isize
}
