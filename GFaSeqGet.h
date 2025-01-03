#ifndef GFASEQGET_H
#define GFASEQGET_H
#include "GFastaIndex.h"

#define MAX_FASUBSEQ 0xF0000000L
//max 4GB sequence data held in memory at a time

class GSubSeq { //virtual subsequence from a larger sequence/chr
 public:
  int64_t sqstart=0; //1-based coord of subseq start on sequence
  int64_t sqlen=0;   //length of subseq loaded
  char* sq=nullptr; //actual subsequence data (end-of-line characters removed)

  GSubSeq(char* p=NULL, int64_t chr_start=0, int64_t seqlen=0) {
     if (p) takeOver(p, chr_start, seqlen);
  }


  void forget() { //forget about pointer data, so we can reuse it
  	sq=NULL;
  	sqstart=0;
  	sqlen=0;
  }
  //take over subseq data, e.g. from GFastaIndex::fetchSeq()
  void takeOver(char* p, int64_t chr_start, int64_t seqlen) {
     clear();
     sq=p;
     sqstart=chr_start;
     sqlen=seqlen;
  }

  void clear() {
     GFREE(sq);
     sqstart=0;
     sqlen=0;
  }

  ~GSubSeq() {
     GFREE(sq);
  }

  //allocates slen+1 memory for subsequence
  // if sovl>0, it will copy the overlap region from previous subseq (old_from, sovl) to (new_to, sovl)
  void setup(int64_t sstart, int64_t slen, int64_t sovl=0, int64_t old_from=0, int64_t new_to=0, int64_t maxseqlen=0);
    //check for overlap with previous window and realloc/extend appropriately
    // the window will keep extending until MAX_FASUBSEQ is reached
};

//
class GFaSeqGet {
  char* fname=nullptr; //file name where the sequence resides (NULL if faidx mode)
  FILE* fh=nullptr;   // stays NULL in faidx mode
  GFastaIndex* faidx=nullptr; //faidx mode if not null, use faidx->fetchSeq() to serve sequence
  off_t fseqstart=-1; //file offset where the sequence actually starts
  int64_t seq_len=0; //total sequence length, if known (when created from GFastaIndex)
  uint line_len=0; //length of each line of text
  uint line_blen=0; //binary length of each line
                 // = line_len + number of EOL character(s)
  GSubSeq* lastsub=nullptr;
  void initialParse(off_t fofs=0, bool checkall=true);
  void initSubseq(int64_t cstart, int64_t clen, int64_t seq_len);
  const char* loadSubseq(int64_t cstart, int64_t& clen);
  void finit(const char* fn, off_t fofs, bool validate);
 public:
  //GStr seqname; //current sequence name
  char* seqname=nullptr;

  GFaSeqGet(const char* fn, off_t fofs, bool validate=false) {
     finit(fn,fofs,validate);
  }

  GFaSeqGet(const char* fn, bool validate=false) {
     finit(fn,0,validate);
  }

  GFaSeqGet(const char* faname, int64_t seqlen, off_t fseqofs, int l_len, int l_blen);
   //indirect GFastaIndex use

  GFaSeqGet(const char* faname, const char* chr, GFastaIndex& faidx);
  //legacy incidental GFastaIndex use (without switching in faidx mode)

  GFaSeqGet(GFastaIndex* fa_idx, const char* seqname=NULL); //full faidx mode

  GFaSeqGet(FILE* f, off_t fofs=0, bool validate=false);

  ~GFaSeqGet() {
    if (fname!=NULL) {
       GFREE(fname);
       fclose(fh);
    }
    GFREE(seqname);
    delete lastsub;
  }

  // returns pointer to internal buffer+offset, not 0-terminated
  const char* seq(int64_t cstart=1, int64_t clen=0) {
	  int cend = clen==0 ? 0 : cstart+clen-1;
	  return getRange(cstart, cend);
  }

  const char* subseq(int64_t cstart, int64_t& clen); //pointer to internal buffer, not 0-terminated
  const char* getRange(int64_t cstart=1, int64_t cend=0, int* retlen=NULL) {
      if (cend==0) cend=(seq_len>0)?seq_len : MAX_FASUBSEQ;
      if (cstart>cend) { Gswap(cstart, cend); }
      int64_t clen=cend-cstart+1;
      const char* r = subseq(cstart, clen);
      if (retlen) *retlen=clen;
      return r;
  }

  // returns new 0-terminated copy, caller is responsible for deallocating the return string
  char* copyRange(int64_t cstart, int64_t cend, bool revCmpl=false, bool upCase=false, int64_t* retlen=NULL);

  // uncached, read and return allocated buffer with a copy of the substring, 0-terminated
  // caller is responsible for deallocating the return string
  char* fetchSeq(int* retlen=NULL) {
   int64_t clen=(seq_len>0) ? seq_len : MAX_FASUBSEQ;
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
    int64_t clen=(seq_len>0) ? seq_len : ((max_len>0) ? max_len : MAX_FASUBSEQ);
    subseq(1, clen);
    }
  void load(uint cstart, uint cend) {
     //cache as much as possible
      if (seq_len>0 && cend>seq_len) cend=seq_len; //correct a bad request
      int64_t clen=cend-cstart+1;
      subseq(cstart, clen);
     }
  int getsublen() { return lastsub!=NULL ? lastsub->sqlen : 0 ; }
  int getseqlen() { return seq_len; } //known when loaded with GFastaIndex
  off_t getseqofs() { return fseqstart; }
  int getLineLen() { return line_len; }
  int getLineBLen() { return line_blen; }
  //reads a subsequence starting at genomic coordinate cstart (1-based)

  size_t getMemoryFootprint() {
      size_t totalSize = sizeof(*this); // Base size of the object
      // Add size of dynamically allocated fname
      if (this->fname != NULL)
          totalSize += strlen(this->fname) + 1; // +1 for the null-terminator
      if (this->seqname != NULL)
          totalSize += strlen(this->seqname) + 1;
      if (this->lastsub != NULL) {
          totalSize += sizeof(GSubSeq); // Size of GSubSeq object itself
          if (this->lastsub->sq != NULL)
              totalSize += this->lastsub->sqlen; // Add the length of the sequence buffer
      }
      return totalSize;
    }
};


//multi-fasta sequence handling
class GFastaDb {
 public:
  char* fastaPath=nullptr; //could be a directory or a multi-fasta file
  GFastaIndex* faIdx=nullptr;
  const char* last_seqname=nullptr;
  GFaSeqGet* faseq=nullptr;

  GFastaDb(const char* fpath=NULL, bool requireIndex=true) {//, bool forceIndexFile=true):fastaPath(NULL), faIdx(NULL), last_seqname(NULL),
     init(fpath, requireIndex);
  }

  void init(const char* fpath, bool requireIndex=true) { //, bool writeIndexFile=true)
     if (fpath==NULL || fpath[0]==0) return;
     last_seqname=NULL;
     int fex=fileExists(fpath);
     if (fex<1)
       GError("Error: file/directory %s does not exist!\n",fpath);
     fastaPath=Gstrdup(fpath);
     if (fex > 1) { // a file
       if (requireIndex) faIdx=new GFastaIndex(fastaPath);
     }
  }

  // get the first sequence from a FASTA file, no index needed
  GFaSeqGet* fetchFirst(const char* fname, bool checkFasta=false) {
	 faseq=new GFaSeqGet(fname, checkFasta);
	 faseq->loadall();
	 //last_fetchid=gseq_id;
	 GFREE(last_seqname);
	 last_seqname=Gstrdup(faseq->seqname);
	 return faseq;
  }


  // when fastaPath is a directory, this function will try to find the FASTA file
  // for the given sequence name, with .fa or .fasta extension
  char* getFastaFile(const char* gseqname) {
	if (fastaPath==NULL) return NULL;
	int gnl=strlen(gseqname);
	char* s=Gstrdup(fastaPath, gnl+8);
	int slen=strlen(s);
	if (s[slen-1]!='/') {//CHPATHSEP ?
	  s[slen]='/';
	  slen++;
	  s[slen]='\0';
	}
	//s.append(gseqname);
    strcpy(s+slen, gseqname);
    slen+=gnl;
	if (!fileExists(s)) {
		//s.append(".fa")
		strcpy(s+slen, ".fa");
		slen+=3;
	}
	if (!fileExists(s)) { strcpy(s+slen, "sta"); slen+=3; }
	if (fileExists(s)) return Gstrdup(s);
	 else {
	   GMessage("Warning: cannot find genomic sequence file %s/%s{.fa,.fasta}\n",fastaPath, s);
	   return NULL;
	 }
	GFREE(s);
  }

 // fetches the whole sequence (chromosome) by its name
 // uses the index if available
 // not a good idea for huge chromosomes
 GFaSeqGet* fetch(const char* gseqname) {
    if (fastaPath==NULL) return NULL;
    if (last_seqname!=NULL && (strcmp(gseqname, last_seqname)==0)
    		&& faseq!=NULL) return faseq;
    delete faseq;
    faseq=NULL;
    GFREE(last_seqname);
    last_seqname=NULL;
    //char* gseqname=GffObj::names->gseqs.getName(gseq_id);
    if (faIdx!=NULL) { //fastaPath was the multi-fasta file name and it must have an index
        faseq=new GFaSeqGet(faIdx, gseqname);
        faseq->loadall(); //just cache the whole sequence, it's faster
        last_seqname=Gstrdup(gseqname);
    }
    else { //fastaPath must be a directory with FASTA files named as gseqname
        char* sfile=getFastaFile(gseqname);
        if (sfile!=NULL) {
      	  faseq=new GFaSeqGet(sfile);
           faseq->loadall();
           GFREE(sfile);
        }
    } //one fasta file per contig
    return faseq;
  }

   ~GFastaDb() {
     GFREE(fastaPath);
     GFREE(last_seqname);
     //delete gcdb;
     delete faIdx;
     delete faseq;
     }
};

// fetch full sequence of a chromosome/FASTA record by seq ID, using an index if available
// -- should not be used for large chromosomes
GFaSeqGet* fastaSeqGet(GFastaDb& gfasta, const char* seqid);

#endif
