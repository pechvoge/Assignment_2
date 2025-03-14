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
#include <sys/mman.h>
#include <sched.h>

int amountDigitsDeterminer(int number);
char* intToASCII(int number);
int getCharArrayLength(const char* array);
char* createLogMsg(int computeTime, int waitTime);

static sigset_t sig_set;

void wait_next_activation(void)
{
    // Waits until the next activation
    unsigned long overruns;
    int err_wait = evl_wait_period(&overruns);
    if (err_wait < 0)
    {
        perror("Waiting for next activation failed.");
    }
}

int start_periodic_timer(uint64_t offset, int period)
{
    // Initialization
    struct itimerspec t;
    timer_t timerid;
    int err_creation, err_settime, err_attach, err_core;

    ktime_t koffset = ktime_set(0, offset*1000); // 1 ms
    ktime_t kperiod = ktime_set(0, period*1000); // 1 ms
   
    // // Sets the timer values
    // t.it_value.tv_sec = offset / 1000000;
    // t.it_value.tv_nsec = (offset % 1000000)*1000;
    // t.it_interval.tv_sec = period / 1000000;
    // t.it_interval.tv_nsec = (period % 1000000)*1000;

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

    int err_period = evl_setperiod(&evl_mono_clock,koffset,kperiod);
    if (err_period < 0)
    {
        perror("Setting period failed.");
        return -1;
    }

    // // Creates the EVL timer with a monotonic clock
    // err_creation = evl_new_timer(EVL_CLOCK_MONOTONIC);
    // if (err_creation < 0)
    // {
    //     perror("Timer creation failed.");
    //     return -1;
    // }

    // // Attaches the current thread to the EVL core
    // err_attach = evl_attach_self("periodic_thread_RT");
    // if (err_attach < 0)
    // {
    //     perror("Attaching to EVL failed.");
    //     return -1;
    // }

    // // Sets the EVL timer
    // err_settime = evl_set_timer(err_creation, &t, NULL);
    // if (err_settime < 0)
    // {
    //     perror("Setting timer failed.");
    //     return -1;
    // }
    printf("Timer started\n");
    return 0;
}

void* periodicThread(void *arg)
{
    // Initialization
    uint64_t offset = 1000; //1 ms
    int period = 1000; //1 ms
    size_t buffer_size = 1024;
    struct timespec start, wait, end;
    int write_fd, proxy_fd, computation_time, waiting_time;
    char *log_msg;
    
    write_fd = open("./timeLog.txt", O_WRONLY|O_CREAT);
    if(write_fd < 0)
        {
            perror("Time log could not be opened.");
        }
    // Creates a proxy for the current thread
    proxy_fd = evl_create_proxy(write_fd,buffer_size,"Realtime proxy");
    
    // Starts the periodic timer with an offset and period of 1ms
    int err_timer = start_periodic_timer(offset, period);
    if (err_timer < 0)
    {   
        perror("Starting periodic timer failed.");
    }

    while (1)
    {
        // Measures time before the computation
        evl_read_clock(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < 1000000; i++){}

        // Measures time after the computation and before the wait function
        evl_read_clock(CLOCK_MONOTONIC, &wait);

        wait_next_activation();
        // Measures time after the wait function
        evl_read_clock(CLOCK_MONOTONIC, &end);

        // Writes the computation time and the waiting time to the file
        computation_time = (wait.tv_sec - start.tv_sec) * 1000000 + (wait.tv_nsec - start.tv_nsec) / 1000;
        waiting_time = (end.tv_sec - wait.tv_sec) * 1000000 + (end.tv_nsec - wait.tv_nsec) / 1000;
        log_msg = createLogMsg(computation_time, waiting_time);
        
        oob_write(proxy_fd, log_msg, sizeof(log_msg));
        oob_write(proxy_fd, log_msg, sizeof(log_msg));
    }
    evl_detach_self();
}

int main(){
    // Locking all current and future memory allocations in RAM
    mlockall(MCL_CURRENT | MCL_FUTURE);

    // Sets scheduling policy to Round Robin and ensures EVL is out of band
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    
    // Creates a new thread
    pthread_t thread;
    pthread_create(&thread, &attr, periodicThread, NULL);

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