#include "GBase.h"
#include "GArgs.h"
#include "GStr.h"
#include "GBitVec.h"
#include "gff.h"

#define USAGE "Usage:\n\
gtest [--bit-test|-g|--genomic-fasta <genomic_seqs_fasta>] [-w|--txseq <txseq.fa>] \n\
  [-x|--txcds <txcds.fa>] [-y|--txprot <txprot.fa>] [-o|--out <outfile_simple.gff>] \n\
  [-t|--test <string>] file1 \n\
 "
enum {
 OPT_HELP=1,
 OPT_GENOMIC,
 OPT_COV,
 OPT_TXSEQ, // transcript sequence, spliced exons only (no introns)
 OPT_TXCDS, // transcript CDS sequence only, if CDS present
 OPT_TXPROT, // protein translation of CDS sequence, if CDS present
 OPT_OUTFILE, // simple gff output, only if specified
 OPT_DISABLE_FLAG,
 OPT_TEST,
 OPT_PID,
 OPT_BITVEC,
 OPT_NUM,
 OPT_REGION
};

GArgsDef opts[] = {
  {"help",          'h', 0, OPT_HELP},
  {"genomic-fasta", 'g', 1, OPT_GENOMIC},
  {"COV",           'c', 1, OPT_COV},
  {"txseq",         'w', 1, OPT_TXSEQ},
  {"txcds",         'x', 1, OPT_TXCDS},
  {"txprot",        'y', 1, OPT_TXPROT},
  {"out",           'o', 1, OPT_OUTFILE},
  {"disable-flag",   0,  0, OPT_DISABLE_FLAG},
  {"test",          't', 1, OPT_TEST},
  {"PID",           'p', 1, OPT_PID},
  {"bit-test",      'B', 0, OPT_BITVEC},
  {"bignum",        'n', 1, OPT_NUM},
  {"region",        'r', 1, OPT_REGION},
  {0,0,0,0} // end of list
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

 // Handle -g/--genomic-fasta option

 GFastaIndex* fastaIndex = nullptr; // fastaIndex
 char* gfile=args.getOpt(OPT_GENOMIC);
 if (gfile) {
   GStr indexFile(gfile);
   indexFile.append(".fai");

   if (fileExists(indexFile.chars())) {
     fastaIndex = new GFastaIndex(gfile, indexFile.chars());
   } else {
     GMessage("Creating FASTA index for %s...\n", gfile);
     fastaIndex=new GFastaIndex(gfile); //loads or creates the index as needed
   }
 }

 // Handle -r/--region option
 if (args.getOpt(OPT_REGION)) {
   if (fastaIndex == nullptr) {
     GError("Error: FASTA index is required for region extraction\n");
   }
   GStr region(args.getOpt(OPT_REGION));
   GStr chr = region;
   // Split chr:start-end
   GStr startEnd = chr.split(':');
   if (startEnd.is_empty()) {
     GError("Error: invalid region format. Expected chr:start-end\n");
   }

   // Split start-end
   GStr endStr = startEnd.split('-');
   if (endStr.is_empty()) {
     GError("Error: invalid region format. Expected chr:start-end\n");
   }

   int start = startEnd.asInt();
   int end = endStr.asInt();
   if (end<start) Gswap(start, end);

   //GFastaRec* rec = fastaIndex->getRecord(chr);
   //if (rec == nullptr) GError("Error: could not get sequence for %s\n", chr.chars());
   //GFaSeqGet seqGet(gfile, rec->seqlen, rec->fpos, rec->line_len, rec->line_blen);
   GFaSeqGet seqGet(gfile, chr.chars(), *fastaIndex);
   int64_t seqlen=0;
   char* sequence=seqGet.copyRange(start, end, false, false, &seqlen);

   if (sequence==nullptr) {
      GError("Error: could not get sequence for region %s\n", region.chars());
   }
   GStr sname(chr);
   sname.appendfmt("|%d-%d", start, end);
   writeFasta(stdout, sname.chars(), NULL, sequence, 60, seqlen);
   GFREE(sequence);
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
  // -- testing gff functions
  if (numargs>1) {
    GMessage("Error: only one GFF/GTF file name can be given at a time!\n");
    exit(1);
  }
  else if (numargs==1) {
    args.startNonOpt(); //reset iteration through non-option arguments
    char* gff_fname=args.nextNonOpt();
    if (fileExists(gff_fname)!=2) {
      GMessage("Error: GFF file %s not found!\n", gff_fname);
      exit(1);
    }
    GffReader* gffr=new GffReader(gff_fname, true, true); //transcripts only, sort chromosomes alphabetically
    //gffr->showWarnings();
    gffr->keepAttrs(false); //no attributes
    gffr->readAll();
    GMessage("GFF file %s parsed %d records:\n", gff_fname, gffr->gflst.Count());
    for (int i=0;i<gffr->gflst.Count();++i) {
      const char* cdsStatus = gffr->gflst[i]->hasCDS() ? "[CDS]" : "[   ]";
      GMessage("%s\t%s\n", gffr->gflst[i]->getID(), cdsStatus);
    }

    // Handle -o output file option
    GStr outfile = args.getOpt(OPT_OUTFILE);
    FILE* fout = NULL;
    if (!outfile.is_empty()) {
      if (outfile == "-") fout = stdout;
      else {
          fout = fopen(outfile.chars(), "w");
          if (fout == NULL)
            GError("Error: could not open output file %s\n", outfile.chars());
      }
      for (int i=0;i<gffr->gflst.Count();++i) {
        gffr->gflst[i]->printGff(fout);
      }
      if (fout != stdout) {
        fclose(fout);
        //GMessage("Wrote %d records to %s\n", gffr->gflst.Count(), outfile.chars());
      }
    }
  // Handle -w output, requires fastaIndex
    char* txseqfile = args.getOpt(OPT_TXSEQ);
    char* txcdsfile = args.getOpt(OPT_TXCDS);
    char* txprotfile = args.getOpt(OPT_TXPROT);
    bool getCDS = (txcdsfile != NULL || txprotfile != NULL);
    if ( txseqfile != NULL || getCDS ) {
      if (fastaIndex == nullptr) {
        GError("Error: FASTA index is required for transcript sequence extraction\n");
      }
      // let's make -w exclusive with -x and -y, keep it simple
      if (txseqfile != NULL && getCDS) {
        GError("Error: -w option is exclusive with -x and/or -y\n");
      }
      FILE* txseqout = NULL;
      FILE* txprotout = NULL;
      if (txcdsfile!=NULL) txseqfile=txcdsfile;
      if (txseqfile) {
        txseqout=stdout;
        if (strcmp(txseqfile, "-")!=0) txseqout = fopen(txseqfile, "w");
        if (txseqout == NULL) {
          GError("Error: could not open transcript sequence output file %s\n", txseqfile);
        }
      }
      if (txprotfile != NULL) {
        txprotout = stdout;
        if (strcmp(txprotfile, "-")!=0) txprotout = fopen(txprotfile, "w");
        if (txprotout == NULL) {
          GError("Error: could not open protein output file %s\n", txprotfile);
        }
      }
      for (int i=0;i<gffr->gflst.Count();++i) {
        GffObj* gobj = gffr->gflst[i];
        if (gobj->exons.Count() == 0) {
          GMessage("Warning: no exons found for %s\n", gobj->getID());
          continue;
        }
        if (getCDS && !gobj->hasCDS()) continue;
        GFaSeqGet seqGet(gfile, gobj->getRefName(), *fastaIndex);
        int seqlen=0;
        char* sequence = gobj-> getSpliced(&seqGet, getCDS, &seqlen);
        if (sequence == nullptr) {
          GMessage("Warning: could not get sequence for %s\n", gobj->getID());
          continue;
        }
        if (txseqout)
           writeFasta(txseqout, gobj->getID(), NULL, sequence, 70, seqlen);
        if (txprotout) {
          // translate to protein and write to file
          int aalen=0;
          char* protseq = translateDNA(sequence, aalen, seqlen);
          if (protseq[aalen-1]=='.') { aalen--; protseq[aalen]='\0'; }//remove stop codon
          //GMessage("Translated %s to protein (%d aa)\n", gobj->getID(), aalen);
          writeFasta(txprotout, gobj->getID(), NULL, protseq, 70, aalen);
          GFREE(protseq);
        }
        GFREE(sequence);
      }
      if (txprotout && txprotout != stdout) fclose(txprotout);
      if (txseqout && txseqout != stdout) fclose(txseqout);
    }
    delete gffr;
  }
  delete fastaIndex;

}
