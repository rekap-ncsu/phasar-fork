#include <string>
#include <cstdio>

int source_func()
{
    return 5;
}

void sink_func(int _conspicuous_arg0)
{}

void entry(int wah) 
{
    sink_func(wah);
    sink_func(source_func());
}

void entry_wrapper()
{
    entry(67);
}