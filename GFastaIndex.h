/*
 * GFastaIndex.h
 *
 *  Created on: Aug 25, 2010
 *      Author: gpertea
 */

#ifndef GFAIDX_H_
#define GFAIDX_H_

#include "GHashMap.hh"
#include "GList.hh"
#include "htslib/faidx.h"
#include "htslib/khash.h"
#include "htslib/kstring.h"

struct GFastaRec {
  const char* seqname=NULL; //only a pointer copy
  long seqlen=0; //sequence length (bases)
  //off_t fpos=0; //offset of the FASTA record start in an uncompressed file
  off_t seq_offset=0; //offset of the first base in the uncompressed file (even for bgzf)
  int line_len=0;  // effective line length (actual bases, excluding EoL characters)
  int line_blen=0; //length of line including EoL characters
  uint64_t bgz_voffs=0; //virtual offset of the first base in the sequence for bgzf
  // --TODO: implement these later, by parsing the file from the last base of the previous record
  //off_t rec_offset=0;  // file offset of the FASTA record start in the uncompressed file
  //off_t rec_bgz_voffs=0; //for compressed files, bgzf virtual offset of the FASTA record start
  GFastaRec(uint slen=0, off_t fp=0, int llen=0, int llenb=0, off_t bgzfp=0):
     seqlen(slen), seq_offset(fp), line_len(llen), line_blen(llenb), bgz_voffs(bgzfp) {
      //fpos=seq_offset; //for legacy index building
     }
  bool operator==(GFastaRec& d) { return (seq_offset==d.seq_offset); }
  bool operator>(GFastaRec& d)  {  return (seq_offset>d.seq_offset);  }
  bool operator<(GFastaRec& d)  {  return (seq_offset<d.seq_offset);  }
};

// -- expose htslib faidx_t structures (from faidx.c)
// WARNING: this might change in future htslib versions
struct g_faidx1_t{
  int id; // faidx_t->name[id] is for this struct.
  uint32_t line_len, line_blen;
  uint64_t len;
  uint64_t seq_offset;
  uint64_t qual_offset;
};
KHASH_MAP_INIT_STR(s, g_faidx1_t)

struct g_faidx_t {
  BGZF *bgzf;
  int n, m;
  char **name;
  khash_t(s) *hash;
  enum fai_format_options format;
};

#define GFAIDX_ERROR_NO_FAIDX "Error: no FASTA index loaded!\n"
//define flags for the format of the index file
// first bit: auto-create index if missing

extern const int GFAIDX_AUTOCREATE;
extern const int GFAIDX_LEGACY; // use old index handling, only for uncompressed files

// for htslib faidx on compressed files, get the bgzf virtual offsets
// this makes the index load a bit slower, but bgz_voffs will be available
// for potential faster bgzf_seek() operations
extern const int GFAIDX_VOFFS;


class GFastaIndex {
  char* fa_name=nullptr;  // could be bgzip-compressed
  char* fai_name=nullptr; // .fai index file name - only needed if not fa_name+".fai"/".gzi"
  char* gzi_name=nullptr; // .gzi index file name - only if user insists on using a different name
  g_faidx_t* fai=nullptr; // stays null if legacyFAI or no index at all
  union {
    unsigned int flags=0;
    struct {
      bool autoCreate:1;
      bool legacyFAI:1;
      bool bgzf_voffs:1;
      bool compressed:1;
    };
  };
  //table of pointers to GFastaRec in the order they were found in the file
  GPVec<GFastaRec> recTable; // recTable(false) - no auto-delete
  int buildIndexLegacy(); //builds the index in memory only
 public:
  GHash<GFastaRec*> records; //keys are sequence names pointers
  //this hash also manages key and GFastaRec (de)allocation

  GFastaRec* getRecord(const char* seqname) {
      return records.Find(seqname);
  }

  bool hasSeq(const char* seqname); //check if sequence exists in index

  const char* getSeqName(int i); //retrieve sequence name by index

  int loadIndex(int set_flags=-1); //override flags here

  bool hasIndex() { return (fai!=nullptr) && records.Count()>0; }
  int storeIndex(); //only for legacy index handling
  int buildIndex() { //unneeded, unless clear()/setFiles() was called after construction
    if (legacyFAI) {
      int rec_count = buildIndexLegacy();
      if (autoCreate && rec_count>0) {//this only directs writing the index file in legacy mode
        if (fai_name==nullptr) { //create from fa_name
          fai_name=Gstrdup(fa_name, 4);
          strcat(fai_name, ".fai");
        }
        storeIndex();
        return rec_count;
      }
    }
    return loadIndex();
  }

  //TODO:  get virtual offset of a specific base location in a sequence
  // this will also allow us to seek to the end of a sequence to read the header of the next FASTA
  //uint64_t getSeqOffset(const char* seqname, int64_t loc=1);

  int getCount() { return records.Count(); }

  void setFiles(const char* fname, const char* fainame=nullptr, const char* gziname=nullptr) {
    clear(); //resets the object
    if (fileExists(fname)!=2) GError("Error: FASTA file %s not found!\n",fname);
    if (fileSize(fname)<=3) GError("Error: invalid fasta file %s !\n",fname);
    fa_name=Gstrdup(fname);
    compressed=endsWith(fa_name,"gz");
    if (fainame) fai_name=Gstrdup(fainame);
    if (gziname) { compressed=true; gzi_name=Gstrdup(gziname); }
    if (!compressed) bgzf_voffs=false;
  }

  GFastaIndex(const char* fname, bool idx_flags=GFAIDX_AUTOCREATE,
       const char* fainame=nullptr, const char* gziname=nullptr):recTable(false),
                                                                 records(true) {
    autoCreate=(idx_flags&GFAIDX_AUTOCREATE)!=0;
    legacyFAI=(idx_flags&GFAIDX_LEGACY)!=0;
    bgzf_voffs=(idx_flags&GFAIDX_VOFFS)!=0;
    setFiles(fname, fainame, gziname);
    loadIndex();
  }

  // for the following fetch methods, the caller is responsible for freeing
  // the returned sequence
  char* fetchSeq(const char* seqname, int64_t cstart, int64_t cend=-1, int64_t* retlen=NULL);

  char* fetchSeq(const char* seqname, int64_t* retlen=NULL) {
    return fetchSeq(seqname, 1, -1, retlen);
  }


  void addRecord(const char* seqname, uint seqlen, off_t foffs, int llen, int llen_full);

  void clear(bool keepNames=false, int set_flags=-1) {
    records.Clear();
    recTable.Clear();
    if (!keepNames) {
      GFREE(fa_name);
      GFREE(fai_name);
      GFREE(gzi_name);
    }
    if (set_flags>=0) {
      autoCreate=(set_flags&GFAIDX_AUTOCREATE)!=0;
      legacyFAI=(set_flags&GFAIDX_LEGACY)!=0;
      bgzf_voffs=(set_flags&GFAIDX_VOFFS)!=0;
    }
    if (fai!=nullptr) {
      fai_destroy((faidx_t*)fai);
      fai=nullptr;
    }
  }

  ~GFastaIndex() {
    clear();
  }
};

#endif /* GFAIDX_H_ */
