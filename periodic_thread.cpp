#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <stdio.h>

static sigset_t sigset;

void wait_next_activation(void)
{
    int sig;
    sigwait(&sigset, &sig);
}

int start_periodic_timer(uint64_t offset, int period)
{
    //initialization
    struct itimerspec t;
    struct sigevent sigev;
    timer_t timerid;
    int err_creation;
    const int signal = SIGALRM;
   
    //start periodic timer
    t.it_value.tv_sec = offset / 1000000;
    t.it_value.tv_nsec = (offset % 1000)*1000;
    t.it_interval.tv_sec = period / 1000000;
    t.it_interval.tv_nsec = (period % 1000000)*1000;

    sigemptyset(&sigset);
    sigaddset(&sigset, signal);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    memset(&sigev, 0, sizeof(struct sigevent));
    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = signal;        

    err_creation = timer_create(CLOCK_REALTIME, &sigev, &timerid);
    if (err_creation < 0)
    {
        perror("Timer creation failed.");
        return -1;
    }
    return timer_settime(timerid, 0, &t, NULL);
}

void* periodicThread(void *arg)
{
    //initialization
    uint64_t offset = 1000; //1ms
    int period = 1000; //1ms

    int err_timer;
    err_timer = start_periodic_timer(offset, period);
    if (err_timer < 0)
    {   
        perror("Starting periodic timer failed.");
    }

    while (1)
    {
        //job body
        wait_next_activation();
    }
}

int main(){
    pthread_t thread;
    pthread_create(&thread, NULL, periodicThread, NULL);
    pthread_join(thread, NULL);
    return 0;
}