#include "GBase.h"
#include "GArgs.h"
#include "GStr.h"
#include "GVec.hh"
#include "GHash.hh"

#define USAGE "Usage:\n\
  htest textfile.. \n\
  \n\
 "
static void strFreeProc(pointer item) {
      GFREE(item);
}

int loadStrings(FILE* f, GPVec<char>& strgsuf, GPVec<char>& strgs) {
  int num=0;
  GLineReader lr(f);
  char* line=NULL;
  while ((line=lr.nextLine())!=NULL) {
	  int len=strlen(line);
	  if (len<5) continue;
	  strgsuf.Add(Gstrdup(line));
	  char* s=Gstrdup(line, line+len-3);
	  strgs.Add(Gstrdup(s));
	  num++;
  } //while line
  return num;
}

int main(int argc, char* argv[]) {
 GPVec<char> strs;
 GPVec<char> sufstrs;
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
   //timing starts here
   for (int i=0;i<strs.Count();i++) {
      thash.fAdd(strs[i], new int(1));
   }
   // timing here
   for (int i=0;i<sufstrs.Count();i++) {
      sufthash.fAdd(sufstrs[i], new int(1));
   }

 }
}
