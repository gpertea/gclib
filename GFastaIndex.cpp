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
      //rec->fofs = val->seq_offset; // uncompressed offset, not useful for compressed files
      rec->seqname = fai->name[i];
      if (compressed && gzi) {
          // clamped binary search: start from bgzf_idx_start
          int ilo = last_found, ihi = gzi->noffs - 1;
          while (ilo <= ihi) {
              int idx_i = (ilo + ihi) / 2; // we could use a better pivot
              if (rec->fofs < gzi->offs[idx_i].uaddr) {
                  ihi = idx_i - 1;
              } else if (rec->fofs >= gzi->offs[idx_i + 1].uaddr) {
                  ilo = idx_i + 1;
              } else {
                  // Found the matching entry, set the compressed offset
                  rec->fofs = gzi->offs[idx_i].caddr;
                  // Set the bgzip in-block offset
                  rec->bgz_ofs = val->seq_offset - gzi->offs[idx_i].uaddr;
                  last_found = idx_i; // Update the lower bound for the next search
                  break;
              }
          }
      } //compressed case
      records.Add(fai->name[i], rec);
  }


  return records.Count();
}



