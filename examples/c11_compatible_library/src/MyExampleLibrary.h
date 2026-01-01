#ifndef MY_EXAMPLE_LIBRARY_H
#define MY_EXAMPLE_LIBRARY_H

/*
 * Example Library Using LogInterface
 * 
 * This library demonstrates C++11 compatible zero-overhead logging.
 * When USE_CUSTOM_LOGGER is not defined, all logging compiles to
 * direct ESP-IDF calls with no Logger dependency.
 */

class MyExampleLibrary {
private:
    int counter;
    unsigned long startTime;
    
public:
    MyExampleLibrary();
    
    void begin();
    void doWork();
    void simulateError(int errorCode);
    void periodicStatus();
};

#endif // MY_EXAMPLE_LIBRARY_H