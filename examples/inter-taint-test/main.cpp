#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void taint_var(char* filename)
{}

void indirect_taint(char* filename)
{
	taint_var(filename);
}

int main(int argc, char** argv) 
{
    char filename[8];
    indirect_taint(filename);
    system(filename);
}