#include "GResUsage.h"

#ifdef __APPLE__
#include <mach/mach.h>
double getMemUsage() {
  double resident_set=0;
  struct task_basic_info t_info;
  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

  if (KERN_SUCCESS == task_info(mach_task_self(),
         TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count)) {
    //vm_usage=double(t_info.virtual_size)/1024;
    resident_set=double(t_info.resident_size)/1024;
    }
// resident size is in t_info.resident_size;
  return resident_set;
}
#endif




// Returns the peak (maximum so far) resident set size (physical
// memory use) measured in bytes
size_t getPeakMemUse() {
#if defined(_WIN32_)
	// -- Windows
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
	return (size_t)info.PeakWorkingSetSize/1024;
#else // defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
	// asssume BSD, Linux, or OSX
	struct rusage rusage;
	getrusage( RUSAGE_SELF, &rusage );
#if defined(__APPLE__) && defined(__MACH__)
	return (size_t)rusage.ru_maxrss/1024;
#else
	return (size_t)(rusage.ru_maxrss);
#endif
#endif
}

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
size_t getCurrentMemUse() {
#if defined(_WIN32_)
	// -- Windows
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
	return (size_t)info.WorkingSetSize;

#elif defined(__APPLE__) && defined(__MACH__)
	// Mac OSX
	struct mach_task_basic_info info;
	mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
	if ( task_info( mach_task_self( ), MACH_TASK_BASIC_INFO,
		(task_info_t)&info, &infoCount ) != KERN_SUCCESS )
		return (size_t)0L;		// Can't access?
	return (size_t)info.resident_size;
#else
	//-- assume Linux
	long progsize = 0L;
	FILE* fp = NULL;
	if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
		return (size_t)0L;		/* Can't open? */
	//if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
	if ( fscanf( fp, "%*s%ld", &progsize ) != 1 )
	{
		fclose( fp );
		return (size_t)0L;		/* Can't read? */
	}
	fclose( fp );
	int page_size=sysconf(_SC_PAGESIZE);
	return ((size_t)progsize * (size_t)page_size)/1024;
}

/*
#else //assume Linux
#include <unistd.h>
#include <string>
#include <ios>
#include <fstream>
void getMemUsage(double& vm_usage, double& resident_set) {
   using std::ios_base;
   using std::ifstream;
   using std::string;
   vm_usage     = 0.0;
   resident_set = 0.0;
   // 'file' stat seems to give the most reliable results
   ifstream stat_stream("/proc/self/stat",ios_base::in);
   // dummy vars for leading entries in stat that we don't care about
   string pid, comm, state, ppid, pgrp, session, tty_nr;
   string tpgid, flags, minflt, cminflt, majflt, cmajflt;
   string utime, stime, cutime, cstime, priority, nice;
   string O, itrealvalue, starttime;

   // the two fields we want
   //
   unsigned long vsize;
   long rss;

   stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
               >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
               >> utime >> stime >> cutime >> cstime >> priority >> nice
               >> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest

   stat_stream.close();

   long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
   vm_usage     = vsize / 1024.0;
   resident_set = rss * page_size_kb;
}
*/

#endif


// get_mem_usage(double &, double &) - takes two doubles by reference,
// attempts to read the system-dependent data for a process' virtual memory
// size and resident set size, and return the results in KB.
//
// On failure, returns 0.0, 0.0
void printMemUsage(FILE* fout) {
  double rs= getCurrentMemUse();
  rs/=1024;
  fprintf(fout, "Resident Size: %6.1fMB\n", rs);
}

double GResUsage::start() {
	started=true;
	stopped=false;
	start_mem=getCurrentMemUse();
	getrusage(RUSAGE_SELF, &start_ru);
	clock_gettime(CLOCK_MONOTONIC, &start_ts);
	double tm=start_ts.tv_sec*1000000.0 + start_ts.tv_nsec/1000.0;
	return tm;
}

double GResUsage::stop() {
	if (started!=true)
		GError("Error: calling GResUsage::stop() without starting it first?\n");
	stopped=true;
	clock_gettime(CLOCK_MONOTONIC, &stop_ts);
	getrusage(RUSAGE_SELF, &stop_ru);
	double tm=stop_ts.tv_sec*1000000.0 + stop_ts.tv_nsec/1000.0;
	stop_mem=getCurrentMemUse();
	return tm;
}

#define RUSAGE_STOPCHECK

void GResUsage::stopCheck(const char* s) {
	if (!started || !stopped)
      GError("Error: GResUsage::%s() cannot be used before start&stop\n", s);
}

double GResUsage::elapsed() {
	stopCheck("elapsed");
	double st=start_ts.tv_sec*1000000.0 + start_ts.tv_nsec/1000.0;
	double et=stop_ts.tv_sec*1000000.0 + stop_ts.tv_nsec/1000.0;
	return (et-st);
}

double GResUsage::u_elapsed() {
	stopCheck("u_elapsed");
	double st=start_ru.ru_utime.tv_sec*1000000.0 + start_ru.ru_utime.tv_usec;
	double et=stop_ru.ru_utime.tv_sec*1000000.0 + stop_ru.ru_utime.tv_usec;
	return (et-st);
}

double GResUsage::s_elapsed() {
	stopCheck("s_elapsed");
	double st=start_ru.ru_stime.tv_sec*1000000.0 + start_ru.ru_stime.tv_usec;
	double et=stop_ru.ru_stime.tv_sec*1000000.0 + stop_ru.ru_stime.tv_usec;
	return (et-st);
}

size_t GResUsage::memoryUsed() {
	stopCheck("memoryUsed");
	return (stop_mem-start_mem);
}

