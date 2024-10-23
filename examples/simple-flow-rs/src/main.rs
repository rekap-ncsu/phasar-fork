// use std::io;
// use std::process::Command;

pub fn main() 
{    
    // let mut conspicuous_input = String::new();
    let conspicuous_input = String::from("wah");
    // io::stdin().read_line(&mut conspicuous_input).unwrap();
    //taint variable `conspicuous_input` here

    let mut conspicuous_arg_str = String::from("echo ");

    conspicuous_arg_str.push_str(&conspicuous_input);

    //put taint check on both `conspicuous_input` and `conspicuous_arg_str` here
    let _conspicuous_sink = conspicuous_arg_str;
}
