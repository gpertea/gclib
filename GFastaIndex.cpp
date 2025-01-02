/*
 * GFastaIndex.cpp
 *
 */

#include "GFastaIndex.h"
#include "htslib/bgzf.h" // Add this line to include the definition of BGZF

//expose BGZF index structures (from bgzf.c)
struct g_bgzidx1_t {
  uint64_t uaddr;  // offset w.r.t. uncompressed data
  uint64_t caddr;  // offset w.r.t. compressed data
};

struct g_bgzidx_t {
  int noffs, moffs;       // the size of the index, n:used, m:allocated
  g_bgzidx1_t *offs;     // offsets
  uint64_t ublock_addr;   // offset of the current block (uncompressed data)
};


bool GFastaIndex::hasSeq(const char* seqname) {
  return faidx_has_seq((faidx_t*)fai, seqname);
}

const char* GFastaIndex::getSeqName(int i) {
  return faidx_iseq((faidx_t*)fai, i);
}

int GFastaIndex::loadIndex(bool autoCreate) {
  if (fa_name==nullptr) GError("Error: no FASTA file name set!\n");
  clear(true); //preserve file names
  //clunky, format could be put into flags and just use fai_load3()
  fai=(g_faidx_t*)fai_load3_format(fa_name, fai_name, gzi_name, autoCreate?FAI_CREATE:0, FAI_FASTA);
  if (!fai) {
    if (autoCreate) GMessage("Error: could not load/create FASTA index for %s\n",fa_name);
    else GMessage("Error: could not load FASTA index for %s\n",fa_name);
    return -1;
  }
  compressed=(fai->bgzf && fai->bgzf->is_compressed);

  /*  //faster traversal than using the kh_get() hash search by name
   for (khint_t k = kh_begin(fai->hash); k != kh_end(fai->hash); ++k) {
      if (!kh_exist(fai->hash, k)) continue;
      g_faidx1_t& rec=kh_val(fai->hash, k);
      const char* seqname=kh_key(fai->hash, k);
      GFastaRec* r=new GFastaRec(rec.len, rec.seq_offset, rec.line_len, rec.line_blen);
      r->seqname=seqname;
      ...
    }
  */
  g_bgzidx_t* gzi = NULL;
  if (compressed) {
     gzi = (g_bgzidx_t*) fai->bgzf->idx; // Access the internal bgzidx_t
  }

  int last_found = 0; // Lower bound of the search space in bgzf_idx

  for (int i = 0; i < fai->n; ++i) {
      khiter_t k = kh_get(s, fai->hash, fai->name[i]);
      if (k == kh_end(fai->hash)) {
          continue;
      }
      g_faidx1_t* val = &kh_val(fai->hash, k);

      GFastaRec* rec = new GFastaRec(val->len, val->seq_offset, val->line_len, val->line_blen);
      rec->seqname = fai->name[i];
      uint64_t seq_offset=val->seq_offset;
      uint64_t caddr=0, inblock_offset=0;
      // locate virtual offset based on seq_offset
      if (compressed && gzi) {
          // clamped binary search: start from last_found
          int ilo = last_found, ihi = gzi->noffs - 1;
          while (ilo <= ihi) {
              int idx_i = (ilo + ihi) / 2; // we could use a better pivot?
              if (seq_offset < gzi->offs[idx_i].uaddr) {
                  ihi = idx_i - 1;
              } else if (seq_offset >= gzi->offs[idx_i + 1].uaddr) {
                  ilo = idx_i + 1;
              } else {
                  // found the matching entry, set the compressed offset
                  caddr = gzi->offs[idx_i].caddr;
                  // set the bgzip in-block offset
                  inblock_offset = val->seq_offset - gzi->offs[idx_i].uaddr;
                  last_found = idx_i; // update the lower bound for the next search
                  break;
              }
          }
       // reassemble the virtual offset from caddr and inblock_offset to store in rec->bgz_voffs
       rec->bgz_voffs = (caddr << 16) | (inblock_offset & 0xFFFF);
      } //compressed case

      records.Add(fai->name[i], rec);
  }


  return records.Count();
}



