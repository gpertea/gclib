#include "GFaSeqGet.h"
#include "gdna.h"
#include <ctype.h>

GFaSeqGet* fastaSeqGet(GFastaDb& gfasta, const char* seqid) {
  if (gfasta.fastaPath==NULL) return NULL;
  return gfasta.fetch(seqid);
}

void GFaSeqGet::finit(const char* fn, off_t fofs, bool validate) {
  fh = fopen(fn, "rb");
  if (fh == NULL) {
    GError("Error (GFaSeqGet) opening file '%s'\n", fn);
  }
  fname = Gstrdup(fn);
  initialParse(fofs, validate);
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


GFaSeqGet::GFaSeqGet(FILE* f, off_t fofs, bool validate) {
  if (f==NULL) GError("Error (GFaSeqGet) : null file handle!\n");
  fh=f;
  initialParse(fofs, validate);
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

void GFaSeqGet::initialParse(off_t fofs, bool checkall) {
 static const char gfa_ERRPARSE[]="Error (GFaSeqGet): invalid FASTA file format.\n";
 if (fofs!=0) { fseeko(fh,fofs,SEEK_SET); } //e.g. for offsets provided by fasta indexing
 //read the first two lines to determine fasta parameters
 GFREE(seqname);
 GDynArray<char> fseqname(64);
 fseqname.DetachPtr();  // will not free the allocated memory
 fseqstart = fofs;
 int c = getc(fh);
 fseqstart++;
 if (c != '>')  // fofs must be at the beginning of a FASTA record!
   GError("Error (GFaSeqGet): not a FASTA record?\n");
 bool getName = true;
 while ((c = getc(fh)) != EOF) {
   fseqstart++;
   if (getName) {
     if (c <= 32)
       getName = false;
     else
       fseqname.Add((char)c);
   }
   if (c == '\n' || c == '\r') {
     break;
   }  // end of defline
 }
 fseqname.Add('\0');       // terminate the string
 seqname = fseqname();  // takeover the string pointer
 if (c == EOF) GError(gfa_ERRPARSE);
 line_len = 0;
 uint lendlen = 0;
 while ((c = getc(fh)) != EOF) {
   if (c == '\n' || c == '\r') {  // end of line encountered
     if (line_len > 0) {         // end of the first "sequence" line
       lendlen++;
       break;
     } else {  // another EoL char at the end of defline
       fseqstart++;
       continue;
     }
   }  // end-of-line characters
   line_len++;
 }
 // we are at the end of first sequence line
 while ((c = getc(fh)) != EOF) {
   if (c == '\n' || c == '\r')
     lendlen++;
   else {
     ungetc(c, fh);
     break;
   }
 }
 line_blen = line_len + lendlen;
 if (c == EOF) return;
 // -- you don't need to check it all if you're sure it's safe
 if (checkall) {  // validate the rest of the FASTA records
   uint llen = 0;  // last line length
   uint elen = 0;  // length of last line ending
   bool waseol = true;
   while ((c = getc(fh)) != EOF) {
     if (c == '>' && waseol) {
       ungetc(c, fh);
       break;
     }
     if (c == '\n' || c == '\r') {
       // eol char
       elen++;
       if (waseol) continue;  // 2nd eol char
       waseol = true;
       elen = 1;
       continue;
     }
     if (c <= 32) GError(gfa_ERRPARSE);  // invalid character encountered
     // --- on a seq char here:
     if (waseol) {  // beginning of a seq line
       if (elen && (llen != line_len || elen != lendlen))
         // GError(gfa_ERRPARSE);
         GError("Error: invalid FASTA format for GFaSeqGet; make sure that\n"
                "the sequence lines have the same length (except last)\n");
       waseol = false;
       llen = 0;
       elen = 0;
     }
     llen++;
   }  // while reading chars
 }    // FASTA checking was requested
 fseeko(fh, fseqstart, SEEK_SET);
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
  if (cstart > seq_len || clen==0) {
    clen = 0;
    return nullptr;
  }
  char* seqdata=nullptr;
  GMALLOC(seqdata, clen);
  if (faidx) {
    int64_t retlen=0;
    char* seq=faidx->fetchSeq(seqname, cstart, cstart+clen-1, &retlen);
    if (seq==nullptr) {
      GFREE(seqdata);
      clen=0;
      return nullptr;
    }
    if (retlen<clen) {
      GREALLOC(seqdata, retlen);
      clen=retlen;
    }
    memcpy(seqdata, seq, retlen);
    GFREE(seq);
    return seqdata;
  }
  //direct file reading mode:
  cstart--; //convert to 0-based offset for file ops
  int64_t lineofs = cstart % line_len;
  off_t f_start = ((int)(cstart/line_len)) * line_blen + lineofs;
  int64_t letters_toread = clen;
  int64_t maxlen = (seq_len>0) ? seq_len-cstart : MAX_CACHE;
  if (letters_toread > maxlen) letters_toread = maxlen;
  int64_t c_end = cstart + letters_toread;
  off_t f_end = ((int)(c_end/line_len)) * line_blen + c_end % line_len;
  int64_t bytes_toRead = f_end - f_start;
  f_start += fseqstart; //absolute file offset
  fseeko(fh, f_start, SEEK_SET);
  char* filebuf=nullptr;
  GMALLOC(filebuf, bytes_toRead);
  int64_t actual_read = fread(filebuf, 1, bytes_toRead, fh);
  if (actual_read == 0) {
    GFREE(filebuf);
    GFREE(seqdata);
    clen = 0;
    return nullptr;
  }
  //copy sequence data from file buffer, skipping line endings
  int eol_size = line_blen - line_len;
  int64_t spos = 0; //position in seqdata
  int64_t fpos = 0; //position in file buffer
  //handle first partial line
  if (lineofs > 0) {
    int64_t cplen = line_len - lineofs;
    if (cplen > letters_toread) cplen = letters_toread;
    if (cplen > actual_read) cplen = actual_read;
    memcpy(seqdata, filebuf, cplen);
    letters_toread -= cplen;
    spos += cplen;
    fpos += cplen + eol_size;
  }
  //copy full lines
  while (letters_toread >= line_len && fpos + line_len <= actual_read) {
    memcpy(&seqdata[spos], &filebuf[fpos], line_len);
    spos += line_len;
    letters_toread -= line_len;
    fpos += line_blen;
  }
  //copy last partial line
  if (fpos < actual_read && letters_toread > 0) {
    int64_t last_copy = (actual_read - fpos < letters_toread) ?
                        actual_read - fpos : letters_toread;
    memcpy(&seqdata[spos], &filebuf[fpos], last_copy);
    spos += last_copy;
  }
  GFREE(filebuf);
  if (spos < clen) {
    GREALLOC(seqdata, spos);
    clen = spos;
  }
  return seqdata;
}

const char* GFaSeqGet::subseq(int64_t n_start, int64_t& n_len, bool clear) {
  static const int MAX_TOTAL_CACHE = 50*1024*1024; //50MB max cache
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
      if (cache_len + prep_len <= MAX_TOTAL_CACHE) {
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
      if (cache_len + app_len <= MAX_TOTAL_CACHE) {
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
      if (cache_len + new_len <= MAX_TOTAL_CACHE) {
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