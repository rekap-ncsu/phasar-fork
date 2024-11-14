fn main() 
{
    let conspicuous_x = source_func();
    sink_func(conspicuous_x);
}

fn source_func() -> Bar
{
    Bar { _conspicuous_y: 98, _conspicuous_z: 5.3 }
}

fn sink_func(_conspicuous_arg0: Bar)
{}

struct Bar
{
    _conspicuous_y: i32,
    _conspicuous_z: f32,
}