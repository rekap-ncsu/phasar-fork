#include <string>
#include <cstdio>

std::string source_func()
{
    return std::string("wah");
}

void sink_func(std::string _conspicuous_arg0)
{}

int main(int argc, char** argv) 
{
    std::string conspicuous_x = source_func();
    sink_func(conspicuous_x);
}
