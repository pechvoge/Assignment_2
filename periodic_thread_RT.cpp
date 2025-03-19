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

int amountDigitsDeterminer(int number);
char* intToASCII(int number);
int getCharArrayLength(const char* array);
char* createLogMsg(int computeTime, int waitTime);

// This function has been adapted from EVL: https://v4.xenomai.org/core/user-api/timer/
void timespec_add_ns(struct timespec *__restrict r,
                  const struct timespec *__restrict t,
             long offset)
{
    long s, ns;

    s = offset / 1000000000;
    ns = offset - s * 1000000000;
    r->tv_sec = t->tv_sec + s;
    r->tv_nsec = t->tv_nsec + ns;
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
        fprintf(stderr, "Timer creation failed: %s\n", strerror(tmfd));
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
    int buffer_size = 1024;
    struct timespec start, wait, end;
    int write_fd, proxy_fd, tmfd;
    int computation_time[1024], waiting_time[1024];
    std::ofstream timeLog("timeLog.txt");
    if(!timeLog.is_open())
    {
        perror("Time log could not be opened.");
    }

    // write_fd = open("./timeLog.txt", O_WRONLY|O_CREAT);
    // if(write_fd < 0)
    //     {
    //         perror("Time log could not be opened.");
    //     }
    // Creates a proxy for the current thread
    //proxy_fd = evl_new_proxy(write_fd,buffer_size,"Realtime proxy");
    
    // Starts the periodic timer with an offset and period of 1ms
    int err_timer = start_periodic_timer(offset, period, &tmfd);
    if (err_timer < 0)
    {   
        perror("Starting periodic timer failed.");
    }

    for (int j =0; j < buffer_size; j++)
    {
        // Measures time before the computation
        evl_read_clock(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < 1000000; i++){}

        // Measures time after the computation and before the wait function
        evl_read_clock(CLOCK_MONOTONIC, &wait);

        wait_next_activation(&tmfd);
        // Measures time after the wait function
        evl_read_clock(CLOCK_MONOTONIC, &end);

        // Writes the computation time and the waiting time to the file
        computation_time[j] = (wait.tv_sec - start.tv_sec) * 1000000 + (wait.tv_nsec - start.tv_nsec) / 1000;
        waiting_time[j] = (end.tv_sec - wait.tv_sec) * 1000000 + (end.tv_nsec - wait.tv_nsec) / 1000;
        // char *log_msg = createLogMsg(computation_time, waiting_time);
        
        // oob_write(proxy_fd, log_msg, sizeof(log_msg));
        // delete[] log_msg;
    }
    // Detaches the current thread from the EVL core
    evl_detach_self();

    // Writes timing data to file
    for (int i = 0; i < buffer_size; i++)
    {
        timeLog << computation_time[i] << "\t"; //ms
        timeLog << waiting_time[i] << "\n"; //ms
    }
    timeLog.close();

    // close(write_fd);
    // close(proxy_fd);
}

int main(){
    // Locking all current and future memory allocations in RAM
    mlockall(MCL_CURRENT | MCL_FUTURE);

    // Sets scheduling policy to Round Robin and ensures EVL is out of band
    // pthread_attr_t attr;
    // pthread_attr_init(&attr);
    // pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    // pthread_attr_setschedpolicy(&attr, SCHED_RR);
    
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

int amountDigitsDeterminer(int number)
{
	if (number == 0)
	{
		return 1;
	}
	
	int amountDigits = 0;
	while (number != 0)
	{
		number /= 10;
		amountDigits++;
	}
	return amountDigits;
}

char* intToASCII(int number)
{
    const int amountDigits = amountDigitsDeterminer(number);
    char* myASCIIchar = new char[amountDigits + 1]; // +1 for null terminator
    for (int i = amountDigits - 1; i >= 0; i--)
    {
        myASCIIchar[i] = static_cast<char>((number % 10) + '0');
        number /= 10;
    }
    myASCIIchar[amountDigits] = '\0';
    return myASCIIchar;
}

int getCharArrayLength(const char* array)
{
    int length = 0;
    while (array[length] != '\0')
    {
        length++;
    }
    return length;
}

char* createLogMsg(int computeTime, int waitTime)
{
    char* computeChar = intToASCII(computeTime);
    int computeCharLength = getCharArrayLength(computeChar);    
    char* waitChar = intToASCII(waitTime);
    int waitCharLength = getCharArrayLength(waitChar);
    
    // Allocate enough space for both strings, a tab, a newline, and the null terminator
    char* logMsg = new char[computeCharLength + waitCharLength + 3];

	// Combine both char arrays into log message
    for (int i = 0; i < computeCharLength; i++)
    {
        logMsg[i] = computeChar[i];
    }
    logMsg[computeCharLength] = '\t';
    for (int i = 0; i < waitCharLength; i++)
    {
        logMsg[computeCharLength + 1 + i] = waitChar[i];
    }
    logMsg[computeCharLength + 1 + waitCharLength] = '\n';
    logMsg[computeCharLength + 1 + waitCharLength + 1] = '\0';
    
    delete[] computeChar;
    delete[] waitChar;
    return logMsg;
}