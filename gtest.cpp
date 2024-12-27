#include "GBase.h"
#include "GArgs.h"
#include "GStr.h"
#include "GBitVec.h"
#include "gff.h"

#define USAGE "Usage:\n\
gtest [--bit-test|-g|--genomic-fasta <genomic_seqs_fasta>] [-c|COV=<cov%>] \n\
 [-s|--seq <seq_info.fsize>] [-o|--out <outfile.gff>] [--disable-flag] [-t|--test <string>]\n\
 [-p|PID=<pid%>] file1 [file2 file3 ..]\n\
 "
enum {
 OPT_HELP=1,
 OPT_GENOMIC,
 OPT_COV,
 OPT_SEQ,
 OPT_OUTFILE,
 OPT_DISABLE_FLAG,
 OPT_TEST,
 OPT_PID,
 OPT_BITVEC,
 OPT_NUM
};

GArgsDef opts[] = {
{"help",          'h', 0, OPT_HELP},
{"genomic-fasta", 'g', 1, OPT_GENOMIC},
{"COV",           'c', 1, OPT_COV},
{"seq",           's', 1, OPT_SEQ},
{"out",           'o', 1, OPT_OUTFILE},
{"disable-flag",   0,  0, OPT_DISABLE_FLAG},
{"test",          't', 1, OPT_TEST},
{"PID",           'p', 1, OPT_PID},
{"bit-test",      'B', 0, OPT_BITVEC},
{"bignum",        'n', 1, OPT_NUM},
{0,0,0,0}
};


int main(int argc, char* argv[]) {
 //GArgs args(argc, argv, "hg:c:s:t:o:p:help;genomic-fasta=COV=PID=seq=out=disable-flag;test=");
 GArgs args(argc, argv, opts);
 fprintf(stderr, "Command line was:\n");
 args.printCmdLine(stderr);
 args.printError(USAGE, true);
 if (args.getOpt(OPT_HELP))
     {
     GMessage("%s\n", USAGE);
     exit(1);
     }

 if (args.getOpt(OPT_NUM)) {
      GStr snum(args.getOpt(OPT_NUM));
      int num=snum.asInt();
      char* numstr=commaprintnum(num);
      GMessage("Number %d written with commas: %s\n", num, numstr);
      GFREE(numstr);
 }
 int numopts=args.startOpt();
 if (numopts)
   GMessage("#### Recognized %d option arguments:\n", numopts);
 int optcode=0;
 while ((optcode=args.nextCode())) {
   char* r=args.getOpt(optcode);
   GMessage("%14s\t= %s\n", args.getOptName(optcode), (r[0]==0)?"True":r);
   }
 int numargs=args.startNonOpt();
 if (numargs>0) {
   GMessage("\n#### Found %d non-option arguments given:\n", numargs);
   char* a=NULL;
   while ((a=args.nextNonOpt())) {
     GMessage("%s\n",a);
     }
   }
 GStr s=args.getOpt('t');
 if (!s.is_empty()) {
    GStr token;
    GMessage("Tokens in \"%s\" :\n",s.chars());
    s.startTokenize(";,: \t");
    int c=1;
    while (s.nextToken(token)) {
      GMessage("token %2d : \"%s\"\n",c,token.chars());
      c++;
      }
    }

}
