fn main() 
{
    let conspicuous_x = source_func();
    sink_func(conspicuous_x);
}

fn source_func() -> i32
{
    98
}

fn sink_func(_conspicuous_arg0: i32)
{}