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
#include <evl/thread.h>
#include <evl/clock.h>
#include <evl/timer.h>
#include <fcntl.h>
#include <evl/proxy.h>
#include <evl/sched.h>
#include <sys/mman.h>
#include <sched.h>

// This function has been adapted from EVL: https://v4.xenomai.org/core/user-api/timer/
// It adds a nanosecond to a timespec to create an offset
void timespec_add_ns(struct timespec *__restrict r,
                  const struct timespec *__restrict t,
             long offset)
{
    long s, ns;

    s = offset / 1000000000;
    ns = offset - s * 1000000000;
    r->tv_sec = t->tv_sec + s;
    r->tv_nsec = t->tv_nsec + ns;

    // If the nanosecond value is greater than 1 second, add 1 second to the second value and subtract 1 second from the nanosecond value
    if (r->tv_nsec >= 1000000000) {
        r->tv_sec++;
        r->tv_nsec -= 1000000000;
    }
}

void wait_next_activation(const int* tmfd)
{
    __u64 ticks;
    // Waits until the next activation
    int rfd = oob_read(*tmfd, &ticks, sizeof(ticks));
    if(rfd < 0)
    {
        perror("Reading from timer failed.");
    }
}

int start_periodic_timer(uint64_t offset, int period, int* tmfd)
{
    // Initialization
    struct itimerspec t;
    struct evl_sched_attrs attrs;
    attrs.sched_policy = SCHED_FIFO;
    attrs.sched_priority = 1;
    timer_t timerid;
    int err_settime, err_attach, err_core, err_sched, err_clock;
    struct timespec current_time;
   
    // Set CPU affinity to core 1 before attaching to EVL core
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set); // clears cpu_set
    CPU_SET(1, &cpu_set); // add CPU core 1 to cpu_set
    err_core = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set); // set CPU affinity
    if (err_core < 0)
    {
        perror("CPU core could not be found.");
        return -1;
    }

    // Attaches the current thread to the EVL core
    err_attach = evl_attach_self("periodic_thread_RT");
    if (err_attach < 0)
    {
        perror("Attaching to EVL failed.");
        return -1;
    }

    err_sched = evl_set_schedattr(err_attach, &attrs);
    if (err_sched != 0)
    {
        fprintf(stderr, "Setting scheduling attributes failed: %s\n", strerror(err_sched));
        return -1;
    }

    // Creates the EVL timer with a monotonic clock
    *tmfd = evl_new_timer(EVL_CLOCK_MONOTONIC);
    if (*tmfd < 0)
    {
        fprintf(stderr, "Timer creation failed: %s\n", strerror(*tmfd));
        return -1;
    }

    // Gets the current time
    err_clock = evl_read_clock(EVL_CLOCK_MONOTONIC, &current_time);
    if (err_clock < 0)
    {
        perror("Reading clock failed.");
        return -1;
    }

    // Sets the timer values
    timespec_add_ns(&t.it_value, &current_time, offset);
    t.it_interval.tv_sec = period / 1000000;
    t.it_interval.tv_nsec = (period % 1000000)*1000;

    // Sets the EVL timer
    err_settime = evl_set_timer(*tmfd, &t, NULL);
    if (err_settime < 0)
    {
        perror("Setting timer failed.");
        return -1;
    }
    evl_printf("Timer started\n");
    return 0;
}

void* periodicThread(void *arg)
{
    // Initialization
    uint64_t offset = 1000; //1 ms
    int period = 1000; //1 ms
    constexpr int buffer_size = 30000;
    struct timespec start, wait, end;
    int tmfd;
    float computation_time[buffer_size], waiting_time[buffer_size];
    std::ofstream timeLog("timeLog.txt");
    if(!timeLog.is_open())
    {
        perror("Time log could not be opened.");
    }
    
    // Starts the periodic timer with an offset and period of 1ms
    int err_timer = start_periodic_timer(offset, period, &tmfd);
    if (err_timer < 0)
    {   
        perror("Starting periodic timer failed.");
    }

    for (int j =0; j < buffer_size; j++)
    {
        // Measures time before the computation
        evl_read_clock(EVL_CLOCK_MONOTONIC, &start);
        for (int i = 0; i < 10000; i++){}

        // Measures time after the computation and before the wait function
        evl_read_clock(EVL_CLOCK_MONOTONIC, &wait);

        wait_next_activation(&tmfd);
        // Measures time after the wait function
        evl_read_clock(EVL_CLOCK_MONOTONIC, &end);

        // Writes the computation time and the waiting time to the file
        computation_time[j] = (wait.tv_sec - start.tv_sec) * 1000000000 + (wait.tv_nsec - start.tv_nsec);
        waiting_time[j] = (end.tv_sec - wait.tv_sec) * 1000000000 + (end.tv_nsec - wait.tv_nsec);
    }

    // Detaches the current thread from the EVL core
    evl_detach_self();

    // Writes timing data to file
    for (int i = 0; i < buffer_size; i++)
    {
        timeLog << computation_time[i] << "\t"; //ns
        timeLog << waiting_time[i] << "\n"; //ns
    }
    timeLog.close();
    return NULL;
}

int main(){
    // Locking all current and future memory allocations in RAM
    mlockall(MCL_CURRENT | MCL_FUTURE);
    
    // Creates a new thread
    pthread_t thread;
    int err = pthread_create(&thread, NULL, periodicThread, NULL);
    if (err != 0) {
        fprintf(stderr, "Thread creation failed: %s\n", strerror(err));
        return -1;
    }

    // Looks whether the new thread is finished
    pthread_join(thread, NULL);
    return 0;
}
