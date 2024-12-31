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

struct GFastaRec {
  const char* seqname=NULL; //only a pointer copy
  long seqlen=0; //sequence length (bases)
  off_t fpos=0; //file offset of the FASTA record
  int line_len=0; //effective line length (actual bases, excluding EoL characters)
  int line_blen=0; //length of line including EoL characters
  GFastaRec(uint slen=0, off_t fp=0, int llen=0, int llenb=0):
    seqlen(slen), fpos(fp), line_len(llen), line_blen(llenb) {
    }
  bool operator==(GFastaRec& d){
      return (fpos==d.fpos);
      }
  bool operator>(GFastaRec& d){
     return (fpos>d.fpos);
     }
  bool operator<(GFastaRec& d){
    return (fpos<d.fpos);
    }

};

class GFastaIndex {
  char* fa_name;
  char* fai_name;
  bool haveFai;
  faidx_t* fai;
 public:
 bool compressed; //true if this is for a compressed fasta file (.gz)
 GHash<GFastaRec*> records;
  void addRecord(const char* seqname, uint seqlen,
                    off_t foffs, int llen, int llen_full);

  GFastaRec* getRecord(const char* seqname) {
    return records.Find(seqname);
    }
  bool hasIndex() { return haveFai; }
  int loadIndex(const char* finame);
  int buildIndex(); //build index in memory by parsing the whole fasta file
  int storeIndex(const char* finame);
  int storeIndex(FILE* fai);
  int getCount() { return records.Count(); }
  GFastaIndex(const char* fname, const char* finame=NULL):records() {
    if (fileExists(fname)!=2) GError("Error: fasta file %s not found!\n",fname);
    if (fileSize(fname)<=0) GError("Error: invalid fasta file %s !\n",fname);
    fa_name=Gstrdup(fname);
    fai_name=finame!=NULL ? Gstrdup(finame) : NULL;
    if (fileSize(fa_name)==0) {
      GError("Error creating GFastaIndex(%s): invalid fasta file!\n",fa_name);
      }
    haveFai=false;
    if (fai_name!=NULL && fileSize(fai_name)>0) {
       //try to load the index file if it exists
       loadIndex(fai_name);
       haveFai=(records.Count()>0);
       }
    }
  ~GFastaIndex() {
    GFREE(fa_name);
    GFREE(fai_name);
    }
};

#endif /* GFAIDX_H_ */
