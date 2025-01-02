/*
 * GFaIdx.h
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
  off_t seq_offset=0; //offset of the first base in the uncompressed file (even for bgzf)
  uint64_t bgz_voffs=0; //virtual offset of the first base in the sequence for bgzf
  int line_len=0;  // effective line length (actual bases, excluding EoL characters)
  int line_blen=0; //length of line including EoL characters
  // --TODO: implement these later, by parsing the file from the last base of the previous record
  //off_t rec_offset=0;  // file offset of the FASTA record start in the uncompressed file
  //off_t rec_bgz_voffs=0; //for compressed files, bgzf virtual offset of the FASTA record start
  GFastaRec(uint slen=0, off_t fp=0, int llen=0, int llenb=0, off_t bgzfp=0):
     seqlen(slen), seq_offset(fp), line_len(llen), line_blen(llenb), bgz_voffs(bgzfp) { }
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

class GFastaIndex {
  char* fa_name=nullptr;  // could be bgzip-compressed
  char* fai_name=nullptr; // .fai index file name - only needed if not fa_name+".fai"/".gzi"
  char* gzi_name=nullptr; // .gzi index file name - only if user insists on using a different name
  g_faidx_t* fai;
 public:
  bool compressed=false; //true if this is for a compressed fasta file (.gz)
  GHashMap<const char*, GFastaRec*> records; //keys are sequence names in fai
  GFastaRec* getRecord(const char* seqname) {
      return records.Find(seqname);
  }
  bool hasSeq(const char* seqname); //check if sequence exists in index
  const char* getSeqName(int i); //retrieve sequence name by index

  int loadIndex(bool autoCreate=true);

  int buildIndex() { //unneeded, unless clear()/setFiles() was called after construction
    return loadIndex(true);
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
    //compressed=endsWith(fa_name,"gz"); let the faidx_load3() figure this out
    if (fainame) fai_name=Gstrdup(fainame);
    if (gziname) gzi_name=Gstrdup(gziname);
  }
  GFastaIndex(const char* fname, bool autoCreate=true, const char* fainame=nullptr,
                                 const char* gziname=nullptr):records() {
    setFiles(fname, fainame, gziname);
    loadIndex(autoCreate);
  }

  void clear(bool keepNames=false) {
    records.Clear();
    if (!keepNames) {
      GFREE(fa_name);
      GFREE(fai_name);
      GFREE(gzi_name);
    }
    if (fai!=NULL) fai_destroy((faidx_t*)fai);
  }

  ~GFastaIndex() {
    clear();
  }
};

#endif /* GFAIDX_H_ */
