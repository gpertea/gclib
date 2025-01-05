#ifndef GFASEQGET_H
#define GFASEQGET_H
#include "GFastaIndex.h"


class GFaSeqGet {
 protected:
  char* fname=nullptr; //file name where the sequence resides (NULL if faidx mode)
  FILE* fh=nullptr;   // stays NULL in faidx mode
  GFastaIndex* faidx=nullptr; //faidx mode if not null, use faidx->fetchSeq() to serve sequence
  off_t fseqstart=-1; //file offset where the sequence actually starts
  int64_t seq_len=0; //total sequence length, if known (when created from GFastaIndex)
  uint line_len=0; //length of each line of text
  uint line_blen=0; //binary length of each line, includes EOL character(s)
  const static int64_t MAX_CACHE = 0xF0000000L;
  GDynArray<char> seq_cache; // cache buffer for the sequence
  int64_t c_start=0; // 1-based start coordinate of the cached region
  int64_t c_end=0;   // 1-based (inclusive) end coordinate of cached sequence
  void initialParse(off_t fofs = 0, bool checkall = true);
  char* loadSeq(int64_t cstart, int64_t& clen);
  void finit(const char* fn, off_t fofs, bool validate);
  void initCache(int64_t cstart, int64_t clen) {
   char* seq;
   if (faidx) {
     seq = faidx->fetchSeq(seqname, cstart, cstart + clen - 1, &clen);
   } else {
     seq = loadSeq(cstart, clen);
   }
   seq_cache.takeOver(seq, clen);
   c_start = cstart;
   c_end = cstart + clen - 1;
 }
 public:
  char* seqname; //curent FASTA record sequence name
  GFaSeqGet(const char* fn, off_t fofs, bool validate = false) {
    finit(fn, fofs, validate);
  }
  GFaSeqGet(const char* fn, bool validate = false) {
    finit(fn, 0, validate);
  }
  GFaSeqGet(const char* faname, int64_t seqlen, off_t fseqofs, int l_len, int l_blen);
  GFaSeqGet(const char* faname, const char* chr, GFastaIndex& faidx);
  GFaSeqGet(GFastaIndex* fa_idx, const char* seqname = NULL);
  GFaSeqGet(FILE* f, off_t fofs = 0, bool validate = false);
  ~GFaSeqGet() {
    if (fname != NULL) {
      GFREE(fname);
      fclose(fh);
    }
    GFREE(seqname);
  }

  //fetch sequence as needed, return sequence pointer (not null-terminated)
  //length is returned in clen
  const char* subseq(int64_t n_start, int64_t& n_len, bool clear = false);

  //get sequence range pointer with optional length return; not null-terminated
  const char* getRange(int64_t cstart = 1, int64_t cend = 0, int* retlen = NULL) {
    if (cend == 0) cend = (seq_len > 0) ? seq_len : MAX_CACHE;
    if (cstart > cend) Gswap(cstart, cend);
    int64_t clen = cend - cstart + 1;
    const char* r = subseq(cstart, clen);
    if (retlen) *retlen = clen;
    return r;
  }
  //returns new allocated copy of sequence range, null terminated
  char* copyRange(int64_t cstart, int64_t cend, bool revCmpl = false, bool upCase = false,
                  int64_t* retlen = NULL);

   //returns a null-terminated copy of the entire chromosome/contig
   char* fetchSeq(int* retlen=nullptr) {
      int64_t clen=(seq_len>0) ? seq_len : MAX_CACHE;
      subseq(1, clen);
      if (retlen) *retlen=clen;
      char* r=NULL;
      GMALLOC(r, clen+1);
      memcpy(r, seq_cache(), clen);
      r[clen]=0;
      return r;
   }

   //fetch from base 1 to seq_len (if known) falling back on max_len or MAX_CACHE
   // (the reading will stop at the end of the FASTA record anyway)
   void loadall(int64_t max_len = 0) {
    int64_t clen = (seq_len > 0) ? seq_len : ((max_len > 0) ? max_len : MAX_CACHE);
    subseq(1, clen);
  }

  void load(int64_t cstart, int64_t cend) {
    if (seq_len > 0 && cend > seq_len) cend = seq_len;
    int64_t clen = cend - cstart + 1;
    subseq(cstart, clen);
  }

  //return sequence pointer (not null-terminated)
  const char* seq(int64_t cstart = 1, int64_t clen = 0) {
      int cend = clen == 0 ? 0 : cstart + clen - 1;
      return getRange(cstart, cend);
  }

  //length of cached sequence starting at c_start
  int64_t getCachedlen() {     return c_end - c_start + 1;  }
  int64_t getCachedStart() {   return c_start;  }
  int64_t getSeqlen() {    return seq_len;  }
  off_t getSeqofs() {    return fseqstart;  }
  int getLineLen() {    return line_len;  }
  int getLineBLen() {    return line_blen;  }
  size_t getMemoryFootprint() {
      size_t totalSize = sizeof(*this); // Base size of the object
      // Add size of dynamically allocated fname
      if (this->fname != NULL)
          totalSize += strlen(this->fname) + 1; // +1 for the null-terminator
      if (this->seqname != NULL)
          totalSize += strlen(this->seqname) + 1;
      totalSize += seq_cache.getCapacity()+sizeof(seq_cache); // Size of GSubSeq object itself
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
