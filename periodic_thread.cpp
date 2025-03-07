#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <stdio.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static sigset_t sig_set;

void wait_next_activation(void)
{
    int sig;
    printf("Waiting for next activation\n");
    int res = sigwait(&sig_set, &sig);
    if (res != 0)
    {
        perror("sigwait failed");
        exit(1);
    }
    printf("Activated\n");
}

int start_periodic_timer(uint64_t offset, int period)
{
    //initialization
    struct itimerspec t;
    struct sigevent sigev;
    timer_t timerid;
    int err_creation;
    const int signal = SIGALRM;
   
    t.it_value.tv_sec = offset / 1000000;
    t.it_value.tv_nsec = (offset % 1000000)*1000;
    t.it_interval.tv_sec = period / 1000000;
    t.it_interval.tv_nsec = (period % 1000000)*1000;

    sigemptyset(&sig_set);
    sigaddset(&sig_set, signal);
    sigprocmask(SIG_BLOCK, &sig_set, NULL);

    memset(&sigev, 0, sizeof(struct sigevent));
    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = signal;        

    err_creation = timer_create(CLOCK_REALTIME, &sigev, &timerid);
    if (err_creation < 0)
    {
        perror("Timer creation failed.");
        return -1;
    }
    int err_settime;
    err_settime = timer_settime(timerid, 0, &t, NULL);
    if (err_settime < 0)
    {
        perror("Setting timer failed.");
        return -1;
    }
    printf("Timer started\n");
    return 0;
}

void* periodicThread(void *arg)
{
    //initialization
    uint64_t offset = 1000; //1ms
    int period = 1000; //1ms

    //start periodic timer
    int err_timer;
    err_timer = start_periodic_timer(offset, period);
    if (err_timer < 0)
    {   
        perror("Starting periodic timer failed.");
    }

    while (1)
    {
        for (int i = 0; i < 1000; i++){}
        wait_next_activation();
    }
}

int main(){
    pthread_t thread;
    pthread_create(&thread, NULL, periodicThread, NULL);
    pthread_join(thread, NULL);
    return 0;
}