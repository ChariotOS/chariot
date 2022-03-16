fn main() {
    println!("cargo:rustc-link-search=/home/nick/chariot/build/root/lib", );

    println!("cargo:rustc-link-lib=c");
}
