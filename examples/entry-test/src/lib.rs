pub fn entry(conspicuous_x: usize) 
{
    sink_func(source_func());
    sink_func(conspicuous_x);
}

fn source_func() -> usize
{
    98
}

pub fn entry_wrapper(_unused: i32)
{
    entry(5);
}

fn sink_func(_conspicuous_arg0: usize)
{}
