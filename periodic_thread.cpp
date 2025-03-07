#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <stdio.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/syscall.h>
#include <unistd.h>
#include <fstream>

static sigset_t sig_set;

bool isPrime(int n) 
{ 
    // Corner case 
    if (n <= 1) 
        return false; 
  
    // Check from 2 to n-1 
    for (int i = 2; i < n; i++) 
        if (n % i == 0) 
            return false; 
  
    return true; 
} 

void wait_next_activation(void)
{
    int sig;
    int res = sigwait(&sig_set, &sig);
    if (res != 0)
    {
        perror("sigwait failed");
        exit(1);
    }
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
    sigev.sigev_notify = SIGEV_THREAD_ID;
    sigev.sigev_signo = signal;
    sigev._sigev_un._tid = syscall(SYS_gettid);

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
    std::ofstream timeLog("timeLog.txt");
    if(!timeLog.is_open())
    {
        perror("Time log could not be opened.");
    }
    struct timespec start, wait, end;

    //start periodic timer
    int err_timer;
    err_timer = start_periodic_timer(offset, period);
    if (err_timer < 0)
    {   
        perror("Starting periodic timer failed.");
    }

    while (1)
    {
        clock_gettime(CLOCK_REALTIME, &start);
        for (int i = 0; i < 100000; i++){}

        clock_gettime(CLOCK_REALTIME, &wait);

        wait_next_activation();
        clock_gettime(CLOCK_REALTIME, &end);

        timeLog << (wait.tv_sec - start.tv_sec) * 1000000 + (wait.tv_nsec - start.tv_nsec) / 1000 << "\t"; //ms
        timeLog << (end.tv_sec - wait.tv_sec) * 1000000 + (end.tv_nsec - wait.tv_nsec) / 1000 << "\n"; //ms
    }
}

int main(){
    pthread_t thread;
    pthread_create(&thread, NULL, periodicThread, NULL);
    pthread_join(thread, NULL);
    return 0;
}