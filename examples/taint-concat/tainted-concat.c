/**
 * example code taken from https://clang.llvm.org/docs/analyzer/checkers.html#optin-taint-generictaint-c-c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void taint_var(char* filename)
{}

// Command Injection Vulnerability Example
int main(int argc, char** argv) 
{
    // char cmd[2048] = "/bin/cat ";
    char cmd[2048];
    char filename[1024];
    int x = 0;
    printf("Filename:");
    // scanf (" %1023[^\n]", filename); // The attacker can inject a shell escape here
    taint_var(filename);
    strcat(cmd, filename);
    system(cmd); // Warning: Untrusted data is passed to a system call
}