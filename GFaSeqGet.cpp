#include "GFaSeqGet.h"
#include "gdna.h"
#include <ctype.h>

#define gfa_ERRPARSE "Error (GFaSeqGet): invalid FASTA record format.\n"

GFaSeqGet* fastaSeqGet(GFastaDb& gfasta, const char* seqid) {
  if (gfasta.fastaPath==NULL) return NULL;
  return gfasta.fetch(seqid);
}

void GFaSeqGet::finit(const char* fn, off_t fofs) {
  fh = fopen(fn, "rb");
  if (fh == NULL) {
    GError("Error (GFaSeqGet) opening file '%s'\n", fn);
  }
  fname = Gstrdup(fn);
  parseRecord(fofs);
  c_start = 0;
}

GFaSeqGet::GFaSeqGet(const char* faname, int64_t seqlen,
  off_t fseqofs, int l_len, int l_blen):seq_cache(seqlen+1) {
 //indirect GFastaIndex use -- the important difference is that
 //the file offset is to the sequence, not to the defline
 // so fseqstart can be directly provided
  fh=fopen(faname,"rb");
  if (fh==NULL) {
    GError("Error (GFaSeqGet) opening file '%s'\n",faname);
    }
  fname=Gstrdup(faname);
  line_len=l_len;
  line_blen=l_blen;
  seq_len=seqlen;
  if (line_blen<line_len)
       GError("Error (GFaSeqGet): invalid line length info (len=%d, blen=%d)\n",
              line_len, line_blen);
  fseqstart=fseqofs;
}

//legacy incidental GFastaIndex use (without switching in faidx mode)
GFaSeqGet::GFaSeqGet(const char* faname, const char* chr, GFastaIndex& faidx) {
  GFastaRec* farec=faidx.getRecord(chr);
  if (farec==NULL) {
    GError("Error (GFaSeqGet): couldn't find FASTA record for '%s'!\n",chr);
    return;
  }
  fh=fopen(faname,"rb");
  if (fh==NULL) {
    GError("Error (GFaSeqGet) opening file '%s'\n",faname);
    }
  fname=Gstrdup(faname);
  line_len=farec->line_len;
  line_blen=farec->line_blen;
  seq_len=farec->seqlen;
  if (line_blen<line_len)
       GError("Error (GFaSeqGet): invalid line length info (len=%d, blen=%d)\n",
              line_len, line_blen);
  fseqstart=farec->seq_offset;
}

//fofs is expected to be the offset of the FASTA RECORD start, not sequence start
GFaSeqGet::GFaSeqGet(FILE* f, off_t fofs) { //checks the FASTA record starting at fofs
  if (f==NULL) GError("Error (GFaSeqGet) : null file handle!\n");
  fh=f;
  parseRecord(fofs);
}


GFaSeqGet::GFaSeqGet(GFastaIndex* fa_idx, const char* sname) { //full faidx mode
  if (fa_idx==NULL) GError("Error (GFaSeqGet): no FASTA index provided!\n");
  if (fa_idx->getCount()==0) GError("Error (GFaSeqGet): empty FASTA index!\n");
  faidx=fa_idx; //faidx mode engaged
  if (sname) {
    GFastaRec* farec=fa_idx->getRecord(sname);
    if (farec==NULL) GError("Error (GFaSeqGet): sequence '%s' not found in FASTA index!\n",sname);
    seqname=Gstrdup(sname);
    seq_len=farec->seqlen;
    line_len=farec->line_len;
    line_blen=farec->line_blen;
    fseqstart=farec->seq_offset;
  }
}


#define CRLF(c) ((c)=='\n' || (c)=='\r')
//parse the entire FASTA record starting at fofs, recording sequence length and line lengths
void GFaSeqGet::parseRecord(off_t fofs, GFastaRec* rec, char** seqptr) {
  if (fofs!=0) { fseeko(fh, fofs, SEEK_SET); } //e.g. for offsets provided by fasta indexing
  //read the first two lines to determine fasta parameters
  GFREE(seqname);
  GDynArray<char> fseqname(64);
  GDynArray<int> line_lengths(2);
  GDynArray<int> line_lends(2);

  GDynArray<char>* seq=seqptr ? new GDynArray<char>(1024) : NULL;
  if (seqptr) *seqptr=NULL;
  fseqname.DetachPtr();  // will not free the allocated memory
  fseqstart = fofs;
  int c = getc(fh);
  fseqstart++;
  if (c != '>') GError(gfa_ERRPARSE); // fofs must be at the beginning of a FASTA record!
  bool getName = true; // in the sequence name
  while ((c = getc(fh)) != EOF) {
    fseqstart++;
    if (getName) {
      if (c <= 32) getName = false;
              else fseqname.Add((char)c);
    }
    if (CRLF(c)) break; // end of defline
  }
  fseqname.Add('\0');       // terminate the string
  seqname = fseqname();  // takeover the string pointer
  if (rec) rec->seqname = seqname;
  if (c == EOF) GError(gfa_ERRPARSE);
  uint llen=0;
  seq_len=0;
  uint lendlen = 0;
  bool eos=false; // end of sequence condition
  while ((c = getc(fh)) != EOF) {
    if (CRLF(c)) {  // end of line encountered
      if (llen) {  // end of a first line of sequence
        lendlen++; // count EoL characters
        line_lengths.Add(llen);
        llen = 0; //reset
        break;
      } else {  // still in defline, another EoL char at the end?
        fseqstart++; // increase offset of the first base in the sequence
        continue;
      }
    }  // EoL char
    //in the first sequence line
    if (c=='>') GError(gfa_ERRPARSE); // '>' in the first line of sequence
    if (seq && c > ' ' && c <= '~') seq->Add((char)c);
    seq_len++;
    llen++;
  }
  // at the end of first sequence line
  if (c == EOF) eos=true;
  if (!eos) {
    bool waseol=CRLF(c);
    // get the rest of the sequence lines if any
    while (!eos && (c = getc(fh)) != EOF) {
      if (CRLF(c)) {
        if (!waseol) { //sequence line just ended
          line_lengths.Add(llen);
          llen = 0; //reset
        }
        waseol=true;
        lendlen++;
      }
      else { // sequence line non-CRLF char
        if (waseol) { // new sequence line
          line_lends.Add(lendlen);
          lendlen=0;
        }
        waseol=false;
        if (c == '>') { // next record
          eos=true;
          break;
        }
        if (seq && c > ' ' && c <= '~') seq->Add((char)c);
        seq_len++;
        llen++;
      }
    }
  }
  if (llen) line_lengths.Add(llen);
  if (lendlen) line_lends.Add(lendlen);
  //check that all lines except last have the same length
  int nlines = line_lengths.Count();
  if (nlines > 2) {
    for (int i = 1; i < nlines-1; i++) {
      if (line_lengths[i] != line_lengths[0])
        GError("Error (GFaSeqGet): inconsistent line lengths in FASTA record %s!\n", seqname);
      if (line_lends[i] != line_lends[0])
        GError("Error (GFaSeqGet): inconsistent line endings in FASTA record %s!\n", seqname);
    }
  }
  line_len = line_lengths[0]; lendlen = line_lends[0];
  line_blen = line_len + lendlen;
  if (rec) {
    rec->line_len = line_len;
    rec->line_blen = line_blen;
    rec->seq_offset = fseqstart;
    rec->seqlen = seq_len;
  }
  if (seqptr) {
    seq->Add('\0');
    *seqptr = seq->DetachPtr();
  }
  delete seq;
  //fseeko(fh, fseqstart, SEEK_SET); // rewind to the beginning of the sequence?
}


//returns a pointer to the subsequence buffer, not 0-terminated
//length is returned in clen; caller does not own the buffer

char* GFaSeqGet::copyRange(int64_t cstart, int64_t cend, bool revCmpl, bool upCase, int64_t* retlen) {
  if (cstart>cend) { Gswap(cstart, cend); }
  int64_t clen=cend-cstart+1;
  const char* gs=subseq(cstart, clen);
  if (gs==NULL) return NULL;
  char* r=NULL;
  GMALLOC(r,clen+1);
  r[clen]=0;
  if (retlen) *retlen=clen;
  memcpy((void*)r,(void*)gs, clen);
  if (revCmpl) reverseComplement(r,clen);
  if (upCase) {
       for (int i=0;i<clen;i++)
            r[i]=toupper(r[i]);
       }
  return r;
 }

 char* GFaSeqGet::loadSeq(int64_t cstart, int64_t& clen) {
  //returns an allocated buffer with the requested sequence
  //cstart is 1-based coordinate
  //updates clen with actual sequence length read
  bool known_seqlen = (seq_len > 0);
  if ((known_seqlen && cstart > seq_len) || clen<=0) {
    clen = 0;
    return nullptr;
  }
  int64_t cend = cstart + clen - 1;
  if (known_seqlen && cend > seq_len) cend = seq_len;
  if (faidx) {
    int64_t retlen=0;
    char* seq=faidx->fetchSeq(seqname, cstart, cend, &retlen);
    if (seq==nullptr) {
      clen=0;
      return nullptr;
    }
    clen=retlen;
    return seq;
  }
  //-- direct file reading mode, code from GFastaIndex::fetchSeq
  if (fh==NULL) GError("Error (GFaSeqGet): no file handle for direct reading!\n");
  int64_t beg=cstart-1, end=cend-1;
  off_t fofs=fseqstart + beg / line_blen * line_len
             + beg % line_blen;
  int ret=fseeko(fh, fofs, SEEK_SET);
  if (ret<0) {
       GMessage("Warning: could not seek to FASTA offset %lld\n for %s", (long long)fofs,
                      seqname);
       clen=0;
       return nullptr;
  }

    size_t l = 0;
    char* seq;
    GMALLOC(seq, (size_t) end - beg + 2);
    int c = 0;;
    size_t maxl = (size_t)end-beg;
    if (known_seqlen) { // we can preallocate
      while ( l < maxl && (c=fgetc(fh))>=0 )
          if (c > ' ' && c <= '~') seq[l++] = c;
      if (c < 0) {
          GMessage("Warning fetchSeq failed to retrieve chunk: %s",
              c == -1 ? "unexpected end of file" : "error reading file");
          GFREE(seq);
          return nullptr;
      }
    } else { // open-ended, must check for end of record
      bool eol=false;
      while ( l < maxl && (c=fgetc(fh))>=0 ) {
        if (c==-1 || (eol && c=='>')) {
          break; // end of file or start of next record
        }
        eol=(c=='\n' || c=='\r');
        if (c > ' ' && c <= '~') seq[l++] = c;
      }
    }
    seq[l] = '\0';
    clen=l;
    return seq;
  }


const char* GFaSeqGet::subseq(int64_t n_start, int64_t& n_len, bool clear) {
  static const int MAX_NONOVL_EXT = 50*1024*1024; //50MB max cache for non-overlapping sequences
  static const int PROXIMITY_THRESHOLD = 1024*1024; //1MB proximity
  if (n_len==0) return nullptr;
  int64_t n_end = n_start + n_len - 1;
  if (clear || seq_cache.Count()==0) {
    initCache(n_start, n_len);
    return seq_cache();
  }
  //check if requested range is within current cache
  if (n_start >= c_start && n_end <= c_end) {
    return seq_cache() + (n_start - c_start);
  }
  //check proximity and overlap scenarios
  int64_t o_start = GMAX(n_start, c_start);
  int64_t o_end = GMIN(n_end, c_end);
  int64_t overlap_len = (o_start <= o_end) ? o_end - o_start + 1 : 0;
  int64_t cache_len = c_end - c_start + 1;

  if (overlap_len > cache_len/3) { //significant overlap
    if (n_start < c_start && n_end <= c_end) { //extend left only
      int64_t prep_len = c_start - n_start;
      if (cache_len + prep_len <= MAX_CACHE) {
        char* prep_seq = loadSeq(n_start, prep_len);
        if (prep_seq) {
          seq_cache.prepend(prep_seq, prep_len);
          GFREE(prep_seq);
          c_start = n_start;
          return seq_cache();
        }
      }
    } else if (n_start >= c_start && n_end > c_end) { //extend right only
      int64_t app_len = n_end - c_end;
      if (cache_len + app_len <= MAX_CACHE) {
        char* app_seq = loadSeq(c_end + 1, app_len);
        if (app_seq) {
          seq_cache.append(app_seq, app_len);
          GFREE(app_seq);
          c_end = n_end;
          return seq_cache() + (n_start - c_start);
        }
      }
    }
  } else if (overlap_len == 0) { //no overlap, check proximity
    int64_t gap = 0;
    if (n_end < c_start)
      gap = c_start - n_end - 1;
    else if (n_start > c_end)
      gap = n_start - c_end - 1;
    if (gap < PROXIMITY_THRESHOLD) {
      int64_t new_len = n_end - n_start + 1;
      if (cache_len + new_len <= MAX_NONOVL_EXT) {
        char* new_seq = loadSeq(n_start, new_len);
        if (new_seq) {
          if (n_end < c_start) {
            seq_cache.prepend(new_seq, new_len);
            c_start = n_start;
          } else {
            seq_cache.append(new_seq, new_len);
            c_end = n_end;
          }
          GFREE(new_seq);
          return seq_cache() + (n_start - c_start);
        }
      }
    }
  }
  //fallback: reinitialize cache
  initCache(n_start, n_len);
  return seq_cache();
}