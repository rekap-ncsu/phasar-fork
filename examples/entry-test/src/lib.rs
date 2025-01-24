pub fn entry(conspicuous_x: usize) 
{
    sink_func(source_func());
    sink_func(make_tainted(conspicuous_x));
}

fn source_func() -> usize
{
    98
}

pub fn entry_wrapper()
{
    entry(5);
}

fn sink_func(_conspicuous_arg0: usize)
{}

fn make_tainted(to_taint: usize) -> usize
{
    to_taint
}