#include "GBase.h"
#include "GArgs.h"
#include "GStr.h"
#include "GVec.hh"
#include "GHash.hh"
#include "GResUsage.h"

#define USAGE "Usage:\n\
  htest textfile.. \n\
  \n\
 "
static void strFreeProc(pointer item) {
      GFREE(item);
}

struct HStrData {
	int cmd; // 0=add, 1=remove, 2=clear
	GStr str;
	HStrData(char* s=NULL, int c=0):cmd(c), str(s) { }
};

int loadStrings(FILE* f, GPVec<HStrData>& strgsuf, GPVec<HStrData>& strgs) {
  int num=0;
  GLineReader lr(f);
  char* line=NULL;
  while ((line=lr.nextLine())!=NULL) {
	  int len=strlen(line);
	  if (len<4) continue;
	  if (strcmp(line, "HCLR")==0) {
		  strgs.Add(new HStrData(NULL, 2));
		  strgsuf.Add(new HStrData(NULL, 2));
		  continue;
	  }
	  if (startsWith(line, "RM ")) {
	     strgsuf.Add(new HStrData(line+3,1) );
	     line[len-3]=0;
	     strgs.Add(new HStrData(line+3,1));
	     continue;
	  }
      strgsuf.Add(new HStrData(line));
      line[len-3]=0;
      strgs.Add(new HStrData(line));
	  num++;
  } //while line
  return num;
}

int main(int argc, char* argv[]) {
 GPVec<HStrData> strs;
 GPVec<HStrData> sufstrs;
 GHash<int> thash;
 GHash<int> sufthash;
 strs.setFreeItem(strFreeProc);
 sufstrs.setFreeItem(strFreeProc);
 //GArgs args(argc, argv, "hg:c:s:t:o:p:help;genomic-fasta=COV=PID=seq=out=disable-flag;test=");
 GArgs args(argc, argv, "h");
 //fprintf(stderr, "Command line was:\n");
 //args.printCmdLine(stderr);
 args.printError(USAGE, true);
 if (args.getOpt('h') || args.getOpt("help")) GMessage(USAGE);
 int numargs=args.startNonOpt();
 if (numargs>0) {
   char* a=NULL;
   FILE* f=NULL;
   int total=0;
   while ((a=args.nextNonOpt())) {
	   f=fopen(a, "r");
	   if (f==NULL) GError("Error: could not open file %s !\n", a);
	   int num=loadStrings(f, sufstrs, strs);
	   total+=num;
   }
   GResUsage swatch;
   //timing starts here
   GMessage("----------------- loading no-suffix strings ----------------\n");
   swatch.start();
   for (int i=0;i<strs.Count();i++) {
	  switch (strs[i]->cmd) {
	    case 0:thash.fAdd(strs[i]->str.chars(), new int(1)); break;
	    case 1:thash.Remove(strs[i]->str.chars()); break;
	    case 2:thash.Clear(); break;
	  }
   }
   swatch.stop();
   GMessage("Elapsed time (microseconds): %.0f us\n", swatch.elapsed());
   GMessage("                  user time: %.0f us\n", swatch.u_elapsed());
   GMessage("                system time: %.0f us\n", swatch.s_elapsed());
   // timing here

   GMessage("----------------- loading suffix strings ----------------\n");
   swatch.start();
   for (int i=0;i<sufstrs.Count();i++) {
		  switch (sufstrs[i]->cmd) {
		    case 0:sufthash.fAdd(sufstrs[i]->str.chars(), new int(1)); break;
		    case 1:sufthash.Remove(sufstrs[i]->str.chars()); break;
		    case 2:sufthash.Clear(); break;
		  }
   }
   swatch.stop();
   GMessage("Elapsed time (microseconds): %.0f us\n", swatch.elapsed());
   GMessage("                  user time: %.0f us\n", swatch.u_elapsed());
   GMessage("                system time: %.0f us\n", swatch.s_elapsed());

 }
}
