#include "GBase.h"
#include "GArgs.h"
#include "GStr.h"
#include "GVec.hh"
#include "GHash.hh"
#include "GIntHash.hh"
#include "GResUsage.h"

#define USAGE "Usage:\n\
  rusage <inputfile> \n\
 Resource usage testing, loading the given file <inputfile>.\n\
 "
int main(int argc, char* argv[]) {
	GArgs args(argc, argv, "h");
	int numargs=args.startNonOpt();
	if (args.getOpt('h') || numargs!=1) {
		GMessage("%s\n",USAGE);
		exit(0);
	}

	char* fname=args.nextNonOpt();
	if (fileExists(fname)!=2) {
		GMessage("Error: file %s not found!\n", fname);
		exit(1);
	}
    GResUsage swatch;
    double mb=((double)getCurrentMemUse())/1024.0;
    GMessage("Total memory in use: %6.1f MBytes\n", mb);
    GMessage("----------------- start timer ----------------\n");
    swatch.start();
    char* allocmem=NULL;
    int allocsize=325*1024*1024;
    GMessage("..........allocating 325MB\n");
    GMALLOC(allocmem, allocsize);
    for (int i=0;i<10;i++)
    	for (int c=0;c<allocsize;c++) {
    		allocmem[c]=i;
    	}
    swatch.stop();
    GMessage("----------------- stopped timer ----------------\n");
    GMessage("Elapsed time (microseconds): %.0f us\n", swatch.elapsed());
    GMessage("                  user time: %.0f us\n", swatch.u_elapsed());
    GMessage("                system time: %.0f us\n", swatch.s_elapsed());
    mb=((double)getCurrentMemUse())/1024.0;
    GMessage("Total memory in use: %6.1f MBytes\n", mb);
    mb=((double)swatch.memoryUsed())/1024.0;
    GMessage("   Memory allocated: %6.1f MBytes\n", mb);
    GFREE(allocmem);
    GMessage("..........freeing 325MB\n");
    mb=((double)getCurrentMemUse())/1024.0;
    GMessage("Total memory in use: %6.1f MBytes\n", mb);
    mb=((double)getPeakMemUse())/1024.0;
    GMessage("----------------- before exiting.. ----------------\n");
    GMessage("  Memory usage peak: %6.1f MBytes\n", mb);
	return 0;
}
