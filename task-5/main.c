#define _POSIX_SOURCE
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int count = 0;

void sa_sigact(int signal, siginfo_t * siginf, void* dummy)
{
    count++;
    if (count > 20) 
        exit(0);
    fprintf(stderr, "Signal reached from process: %d\n", siginf->si_pid);
}

int main(int argc, char** argv)
{
    int i;
    struct sigaction sigact;
    sigact.sa_sigaction = sa_sigact;
    sigact.sa_flags = SA_SIGINFO;
    for (i = 0; i < 40; i++)
        sigaction(i, &sigact, NULL);
    for (;;);
    return 0;
}
