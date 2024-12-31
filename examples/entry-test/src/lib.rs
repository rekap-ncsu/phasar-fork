pub fn entry(conspicuous_x: usize) 
{
    sink_func(conspicuous_x);
}

fn source_func() -> usize
{
    98
}

pub fn entry_wrapper()
{
    entry(5);
    sink_func(source_func())
}

fn sink_func(_conspicuous_arg0: usize)
{}
