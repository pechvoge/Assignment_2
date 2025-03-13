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

int amountDigitsDeterminer(int number);
char* intToASCII(int number);
int getCharArrayLength(const char* array);
char* createLogMsg(int computeTime, int waitTime);

static sigset_t sig_set;

void wait_next_activation(void)
{
    // Waits for a pending asynchronous signal 
    int sig;
    int res = sigwait(&sig_set, &sig);
    if (res != 0)
    {
        perror("Sigwait failed");
        exit(1);
    }
}

int start_periodic_timer(uint64_t offset, int period)
{
    // Initialization
    struct itimerspec t;
    struct sigevent sigev;
    timer_t timerid;
    int err_creation, err_settime;
    const int signal = SIGALRM;
   
    // Sets the timer values
    t.it_value.tv_sec = offset / 1000000;
    t.it_value.tv_nsec = (offset % 1000000)*1000;
    t.it_interval.tv_sec = period / 1000000;
    t.it_interval.tv_nsec = (period % 1000000)*1000;

    // Creates an empty set and adds a signal to it and blocks this signal  
    sigemptyset(&sig_set);
    sigaddset(&sig_set, signal);
    sigprocmask(SIG_BLOCK, &sig_set, NULL);

    // Initializes the sigevent structure
    memset(&sigev, 0, sizeof(struct sigevent));    

    // Setup the sigevent structure to notify the thread with current ID
    sigev.sigev_notify = SIGEV_THREAD_ID;
    sigev.sigev_signo = signal;
    sigev._sigev_un._tid = syscall(SYS_gettid);

    // Creates the timer with a monotonic clock
    err_creation = timer_create(CLOCK_MONOTONIC, &sigev, &timerid);
    if (err_creation < 0)
    {
        perror("Timer creation failed.");
        return -1;
    }

    // Sets the timer
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
    // Initialization
    uint64_t offset = 1000; //1 ms
    int period = 1000; //1 ms
    int waitTime, computeTime;
    struct timespec start, wait, end;
    std::ofstream timeLog("timeLog.txt");
    if(!timeLog.is_open())
    {
        perror("Time log could not be opened.");
    }
    
    // Starts the periodic timer with an offset and period of 1ms
    int err_timer;
    err_timer = start_periodic_timer(offset, period);
    if (err_timer < 0)
    {   
        perror("Starting periodic timer failed.");
    }

    while (1)
    {
        // Measures time before the computation
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < 1000000; i++){}

        // Measures time after the computation and before the sigwait function
        clock_gettime(CLOCK_MONOTONIC, &wait);

        wait_next_activation();
        // Measures time after the sigwait function
        clock_gettime(CLOCK_MONOTONIC, &end);

        // Writes the computation time and the waiting time to the file
        computeTime = (wait.tv_sec - start.tv_sec) * 1000000 + (wait.tv_nsec - start.tv_nsec) / 1000; //ms
        waitTime = (end.tv_sec - wait.tv_sec) * 1000000 + (end.tv_nsec - wait.tv_nsec) / 1000; //ms 
        timeLog << createLogMsg(computeTime, waitTime);
        //timeLog << computeTime << "\t"; //ms
        //timeLog << waitTime << "\n"; //ms
    }
}

int main(){
    // Creates a new thread
    pthread_t thread;
    pthread_create(&thread, NULL, periodicThread, NULL);

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
    myASCIIchar[amountDigits] = '\0'; // null terminator
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