#ifndef _GRESUSAGE
#define _GRESUSAGE_
#include "GBase.h"
#include <sys/resource.h>
#include <time.h>

// report the memory usage of the current process, rounded to kilobytes
size_t getCurrentMemUse(); //current memory usage of the program (RSS)
size_t getPeakMemUse(); //maximum memory usage for the program at the time of the call

void printMemUsage(FILE* fout=stderr);

//get a timer counter in microseconds (using clock_gettime(CLOCK_MONOTONIC )
double get_usecTime();

class GResUsage {
  protected:
	bool started;
	bool stopped;
	size_t start_mem;
	size_t stop_mem;
	struct rusage start_ru;
	struct rusage stop_ru;
	struct timespec start_ts;
	struct timespec stop_ts;
	void stopCheck(const char* s);
  public:
	GResUsage(bool do_start=false):started(false),
	   stopped(false), start_mem(0), stop_mem(0),
	   start_ts({0, 0}), stop_ts({0,0}) {
          if (do_start) start();
	}

	double start(); //returns microseconds time using clock_gettime(CLOCK_MONOTONIC
	double stop(); //stop the stopwatch, returns the current time in microseconds
	double elapsed(); //microseconds elapsed between start and stop (wallclock time)
	double u_elapsed(); //microseconds of user time elapsed
	double s_elapsed(); //microseconds of system time elapsed
	size_t memoryUsed(); //memory increase between start and stop in KB (can be negative)
};

#endif
