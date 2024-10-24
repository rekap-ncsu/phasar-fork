use std::io;
use std::process::Command;

pub fn main() 
{    
    let mut input = String::new();
    // let input = String::from("wah");
    io::stdin().read_line(&mut input).unwrap();
    //taint variable `input` here

    let mut cmd = Command::new("sh");

    //put taint check here
    let cmd = cmd.arg("-c")
        .arg(format!("echo \"Totally safe: {}\"", input));

    // println!("args: {:?}", cmd.get_args()); //a call to get_args(), which dependes on a foreign (non-rust) function internally

    cmd.status()
        .expect("the program errored!");

    // cmd.current_dir("..");
    // panic!("panic! panic!");
}
