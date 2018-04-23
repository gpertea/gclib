#ifndef GFASEQGET_H
#define GFASEQGET_H
#include "GFastaIndex.h"
#include "GStr.h"

#define MAX_FASUBSEQ 0x20000000
//max 512MB sequence data held in memory at a time

class GSubSeq {
 public:
  uint sqstart; //1-based coord of subseq start on sequence
  uint sqlen;   //length of subseq loaded
  char* sq; //actual subsequence data will be stored here
                // (with end-of-line characters removed)

  /*char* xseq; //the exposed pointer to the last requested subsequence start
  off_t xstart; //the coordinate start for the last requested subseq
  off_t xlen; //the last requested subseq len*/
  GSubSeq() {
     sqstart=0;
     sqlen=0;
     sq=NULL;
     /* xseq=NULL;
     xstart=0;
     xlen=0;*/
     }
  void forget() { //forget about pointer data, so we can reuse it
  	sq=NULL;
  	sqstart=0;
  	sqlen=0;
  }
  ~GSubSeq() {
     GFREE(sq);
     }
  // genomic, 1-based coordinates:
  void setup(uint sstart, int slen, int sovl=0, int qfrom=0, int qto=0, uint maxseqlen=0);
    //check for overlap with previous window and realloc/extend appropriately
    //returns offset from seq that corresponds to sstart
    // the window will keep extending until MAX_FASUBSEQ is reached
};

//
class GFaSeqGet {
  char* fname; //file name where the sequence resides
  FILE* fh;
  off_t fseqstart; //file offset where the sequence actually starts
  uint seq_len; //total sequence length, if known (when created from GFastaIndex)
  int line_len; //length of each line of text
  int line_blen; //binary length of each line
                 // = line_len + number of EOL character(s)
  GSubSeq* lastsub;
  void initialParse(off_t fofs=0, bool checkall=true);
  const char* loadsubseq(uint cstart, int& clen);
  void finit(const char* fn, off_t fofs, bool validate);
 public:
  GStr seqname; //current sequence name
  GFaSeqGet(): fname(NULL), fh(NULL), fseqstart(0), seq_len(0),
		  line_len(0), line_blen(0), lastsub(NULL), seqname("",42) {
  }

  GFaSeqGet(const char* fn, off_t fofs, bool validate=false):fname(NULL), fh(NULL),
		    fseqstart(0), seq_len(0), line_len(0), line_blen(0),
			lastsub(NULL), seqname("",42) {
     finit(fn,fofs,validate); 
  }

  GFaSeqGet(const char* fn, bool validate=false):fname(NULL), fh(NULL),
		    fseqstart(0), seq_len(0), line_len(0), line_blen(0),
			lastsub(NULL), seqname("",42) {
     finit(fn,0,validate);
  }

  GFaSeqGet(const char* faname, uint seqlen, off_t fseqofs, int l_len, int l_blen);
  //constructor from GFastaIndex record

  GFaSeqGet(FILE* f, off_t fofs=0, bool validate=false);

  ~GFaSeqGet() {
    if (fname!=NULL) {
       GFREE(fname);
       fclose(fh);
    }
    delete lastsub;
  }
  const char* subseq(uint cstart, int& clen);
  const char* getRange(uint cstart=1, uint cend=0) {
      if (cend==0) cend=(seq_len>0)?seq_len : MAX_FASUBSEQ;
      if (cstart>cend) { Gswap(cstart, cend); }
      int clen=cend-cstart+1;
      //int rdlen=clen;
      return subseq(cstart, clen);
      }

  //caller is responsible for deallocating the return string
  char* copyRange(uint cstart, uint cend, bool revCmpl=false, bool upCase=false);

  //uncached, read and return allocated buffer
  //caller is responsible for deallocating the return string
  char* fetchSeq(int* retlen=NULL) {
  	int clen=(seq_len>0) ? seq_len : MAX_FASUBSEQ;
  	if (lastsub) { delete lastsub; lastsub=NULL; }
  	subseq(1, clen);
  	if (retlen) *retlen=clen;
  	char* r=lastsub->sq;
  	lastsub->forget();
  	if (clen>0) {
  	   r[clen]=0;
  	}
  	else {
  		r=NULL;
  	}
  	return r;
  }

  void loadall(uint32 max_len=0) {
    //TODO: better read the whole sequence differently here - line by line
    //so when EOF or another '>' line is found, the reading stops!
    int clen=(seq_len>0) ? seq_len : ((max_len>0) ? max_len : MAX_FASUBSEQ);
    subseq(1, clen);
    }
  void load(uint cstart, uint cend) {
     //cache as much as possible
      if (seq_len>0 && cend>seq_len) cend=seq_len; //correct a bad request
      int clen=cend-cstart+1;
      subseq(cstart, clen);
     }
  int getsublen() { return lastsub!=NULL ? lastsub->sqlen : 0 ; }
  int getseqlen() { return seq_len; } //known when loaded with GFastaIndex
  off_t getseqofs() { return fseqstart; }
  int getLineLen() { return line_len; }
  int getLineBLen() { return line_blen; }
  //reads a subsequence starting at genomic coordinate cstart (1-based)
 };

//multi-fasta sequence handling
class GFastaDb {
 public:
  char* fastaPath;
  GFastaIndex* faIdx; //could be a cdb .cidx file
  //int last_fetchid;
  const char* last_seqname;
  GFaSeqGet* faseq;
  //GCdbYank* gcdb;
  GFastaDb(const char* fpath=NULL, bool forceIndexFile=true):fastaPath(NULL), faIdx(NULL), last_seqname(NULL),
		  faseq(NULL) {
     //gcdb=NULL;
     init(fpath, forceIndexFile);
  }

  void init(const char* fpath, bool writeIndexFile=true) {
     if (fpath==NULL || fpath[0]==0) return;
     //last_fetchid=-1;
     last_seqname=NULL;
     if (!fileExists(fpath))
       GError("Error: file/directory %s does not exist!\n",fpath);
     fastaPath=Gstrdup(fpath);
     GStr gseqpath(fpath);
     if (fileExists(fastaPath)>1) { //exists and it's not a directory
            GStr fainame(fastaPath);
            if (fainame.rindex(".fai")==fainame.length()-4) {
               //.fai index file given directly
               fastaPath[fainame.length()-4]=0;
               if (!fileExists(fastaPath))
                  GError("Error: cannot find fasta file for index %s !\n", fastaPath);
            }
             else fainame.append(".fai");
            //GMessage("creating GFastaIndex with fastaPath=%s, fainame=%s\n", fastaPath, fainame.chars());
            faIdx=new GFastaIndex(fastaPath,fainame.chars());
            GStr fainamecwd(fainame);
            int ip=-1;
            //if ((ip=fainamecwd.rindex(CHPATHSEP))>=0)
            if ((ip=fainamecwd.rindex('/'))>=0)
               fainamecwd.cut(0,ip+1);
            if (!faIdx->hasIndex()) { //could not load index file .fai
               //try current directory (danger, might not be the correct index for that file!)
               if (fainame!=fainamecwd) {
                 if (fileExists(fainamecwd.chars())>1) {
                    faIdx->loadIndex(fainamecwd.chars());
                  }
               }
            } //tried to load index
            if (!faIdx->hasIndex()) { //no index file to be loaded, build the index
                 //if (forceIndexFile)
                 //   GMessage("No fasta index found for %s. Rebuilding, please wait..\n",fastaPath);
                 faIdx->buildIndex(); //build index in memory only
                 if (faIdx->getCount()==0) GError("Error: no fasta records found!\n");
                 if (writeIndexFile) {
                     //GMessage("Fasta index rebuilt.\n");
                	 GStr idxfname(fainame);
                     FILE* fcreate=fopen(fainame.chars(), "w");
                     if (fcreate==NULL) {
                        GMessage("Warning: cannot create fasta index file %s! (permissions?)\n", fainame.chars());
                        if (fainame!=fainamecwd) {
                        	idxfname=fainamecwd;
                        	GMessage("   Attempting to create it in the current directory..\n");
                        	if ((fcreate=fopen(fainamecwd.chars(), "w"))==NULL)
                        		GError("Error: cannot create fasta index file %s!\n", fainamecwd.chars());
                        }
                     }
                     if (fcreate!=NULL) {
                    	 if (faIdx->storeIndex(fcreate)<faIdx->getCount())
                              GMessage("Warning: error writing the index file %s!\n", idxfname.chars());
                    	 else GMessage("FASTA index file %s created.\n", idxfname.chars());
                     }
                 } //file storage of index requested
            } //creating FASTA index
    } //multi-fasta file
  }

  GFaSeqGet* fetchFirst(const char* fname, bool checkFasta=false) {
	 faseq=new GFaSeqGet(fname, checkFasta);
	 faseq->loadall();
	 //last_fetchid=gseq_id;
	 last_seqname=faseq->seqname.chars();
	 return faseq;
  }

  char* getFastaFile(const char* gseqname) {
	if (fastaPath==NULL) return NULL;
	GStr s(fastaPath);
	s.trimR('/');
	s.append('/');
	s.append(gseqname);
	GStr sbase(s);
	if (!fileExists(s.chars())) s.append(".fa");
	if (!fileExists(s.chars())) s.append("sta");
	if (fileExists(s.chars())) return Gstrdup(s.chars());
	 else {
	   GMessage("Warning: cannot find genomic sequence file %s{.fa,.fasta}\n",sbase.chars());
	   return NULL;
	 }
  }

 GFaSeqGet* fetch(const char* gseqname) {
    if (fastaPath==NULL) return NULL;
    if (last_seqname!=NULL && (gseqname==last_seqname || strcmp(gseqname, last_seqname)==0)
    		&& faseq!=NULL) return faseq;
    delete faseq;
    faseq=NULL;
    //last_fetchid=-1;
    last_seqname=NULL;
    //char* gseqname=GffObj::names->gseqs.getName(gseq_id);
    if (faIdx!=NULL) { //fastaPath was the multi-fasta file name and it must have an index
        GFastaRec* farec=faIdx->getRecord(gseqname);
        if (farec!=NULL) {
             faseq=new GFaSeqGet(fastaPath,farec->seqlen, farec->fpos,
                               farec->line_len, farec->line_blen);
             faseq->loadall(); //just cache the whole sequence, it's faster
             //last_fetchid=gseq_id;
             last_seqname=gseqname;
        }
        else {
          GMessage("Warning: couldn't find fasta record for '%s'!\n",gseqname);
          return NULL;
        }
    }
    else { //directory with FASTA files named as gseqname
        char* sfile=getFastaFile(gseqname);
        if (sfile!=NULL) {
      	   faseq=new GFaSeqGet(sfile);
           faseq->loadall();
           //last_fetchid=gseq_id;
           GFREE(sfile);
           }
    } //one fasta file per contig

    //else GMessage("Warning: fasta index not available, cannot retrieve sequence %s\n",
    //		gseqname);
    return faseq;
  }

   ~GFastaDb() {
     GFREE(fastaPath);
     //delete gcdb;
     delete faIdx;
     delete faseq;
     }
};


GFaSeqGet* fastaSeqGet(GFastaDb& gfasta, const char* seqid);

#endif
