#include "types.h"
#include "stat.h"
#include "user.h"
#include "trace.h"

void forktest() {
    int fr = fork();
    if(fr == 0) {
        close(open("README.md", 0));
        exit();
    } 
    else {
        wait();
    }
}

int main() {
    trace(T_TRACE);
    forktest();
    trace(T_UNTRACE);

    trace(T_TRACE | T_ONFORK);
    forktest();

    trace(T_UNTRACE);

    exit();
}