Assignment 2.1
-----------------------------------------
Run
    In a terminal run the following commands to compile and run:
    g++ periodic_thread.cpp -o periodic_thread -pthread -lpthread
    ./periodic_thread

    For extra computational load by 50 workers on the CPU run the following in a new terminal:
    stress --cpu 50

Core Components
    wait_next_activation(): waits for a pending signal from the periodic timer using sigwait
    start_periodic_timer(): sets up signal events and creates timer using timer_create and starts timer
    periodicThread(): main function of new thread for periodic timer in which the while loop is ran and timing performance is logged to a file
    main(): creates a new thread and waits until the thread is done executing

Assignment 2.2
-----------------------------------------
Run
    In a terminal run the following commands to compile and run:
    g++ periodic_thread.cpp -o periodic_thread $(pkg-config /usr/evl/lib/pkgconfig/evl.pc --cflags --libs) -pthread -lpthread
    sudo ./periodic_thread

    For extra computational load by 50 workers on the CPU run the following in a new terminal:
    stress --cpu 50

Core Components
    wait_next_activation(): waits for a pending signal from the periodic timer using oob_read
    start_periodic_timer(): attaches the new thread to the EVL core, sets scheduling attributes, creates and runs a periodic timer
    periodicThread(): main function of new thread for periodic timer in which the while loop is ran and timing performance is logged to a file
    main(): locks all memory allocations in RAM, creates a new thread and waits until the thread is done executing

