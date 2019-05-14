#ifndef _GRESUSAGE
#define _GRESUSAGE_
#include "GBase.h"
#include <time.h>
// a Linux-specific way to report the memory usage of the current process
void getMemUsage(double& vm_usage, double& resident_set);

void printMemUsage(FILE* fout=stderr);

//get a timer counter in microseconds (using clock_gettime(CLOCK_MONOTONIC )
double get_usecTime();

class GResUsage {
  protected:
	struct rusage start_ru;
	struct rusage end_ru;
	struct timespec start_ts;
	struct timespec end_ts;
  public:
	double start(); //returns microseconds time using clock_gettime(CLOCK_MONOTONIC
	double stop(); //stop the stopwatch
	double elapsed(); //microseconds elapsed between start and stop;
	int memoryUsed(); //memory increase between start and stop in KB (can be negative)

};



#endif
