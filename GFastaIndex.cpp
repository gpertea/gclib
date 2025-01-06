/*
 * GFastaIndex.cpp
 *
 */

#include "GFastaIndex.h"
#include "htslib/bgzf.h"

const int GFAIDX_AUTOCREATE=0x01;
const int GFAIDX_LEGACY    =0x02;
const int GFAIDX_VOFFS     =0x04;

#define ERR_FAIDXLINE "Error parsing fasta index line: \n%s\n"
#define ERR_FASTA_OPEN "Error opening FASTA file: %s\n"
#define ERR_FAIDX_OPEN "Error opening FASTA file: %s\n"
#define ERR_FALINELEN "Error: sequence lines in a FASTA record must have the same length!\n"
#define ERR_FAI_LEGACY_COMPRESSED "Error: legacy FASTA index handling is not supported for compressed files!\n"
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
  if (legacyFAI) return records.Find(seqname)!=nullptr;
  return faidx_has_seq((faidx_t*)fai, seqname);
}

const char* GFastaIndex::getSeqName(int i) {
  return faidx_iseq((faidx_t*)fai, i);
}

int GFastaIndex::buildIndexLegacy() {
    if (compressed) GError(ERR_FAI_LEGACY_COMPRESSED);
    //this parses the whole fasta file, so it could be slow for large files
    //builds the index in memory only
    if (fa_name==NULL)
       GError("Error: GFastaIndex::buildIndex() called with no fasta file!\n");
    FILE* fa=fopen(fa_name,"rb");
    if (fa==NULL) GError(ERR_FASTA_OPEN, fa_name);
    records.Clear();
    GLineReader fl(fa);
    char* s=NULL;
    uint seqlen=0;
    int line_len=0,line_blen=0;
    bool newSeq=false; //set when FASTA header is encountered
    off_t newSeqOffset=0;
    //int prevOffset=0;
    char* seqname=NULL;
    int last_len=0;
    bool mustbeLastLine=false; //true if the line length decreases
    while ((s=fl.nextLine())!=NULL) {
      if (s[0]=='>') {
        if (seqname!=NULL) {
          if (seqlen==0)
            GError("Warning: empty FASTA record skipped (%s)!\n",seqname);
          else { //seqlen!=0
            addRecord(seqname, seqlen,newSeqOffset, line_len, line_blen);
            }
          }
        char *p=s;
        while (*p > 32) p++;
        *p=0;
        GFREE(seqname);
        seqname=Gstrdup(&s[1]);
        newSeq=true;
        newSeqOffset=fl.getfpos();
        last_len=0;
        line_len=0;
        line_blen=0;
        seqlen=0;
        mustbeLastLine=false;
      } //defline parsing
      else { //sequence line
        int llen=fl.tlength();
        int lblen=fl.blength(); //fl.getFpos()-prevOffset;
        if (newSeq) { //first sequence line after defline
          line_len=llen;
          line_blen=lblen;
        }
        else {//next seq lines after first
          if (mustbeLastLine) {
              //could be empty line, adjust for possible spaces
              if (llen>0) {
                char *p=s;
                //trim spaces, tabs etc. on the last line
                while (*p > 32) ++p;
                llen=(p-s);
              }
              if (llen>0) GError(ERR_FALINELEN);
          }
          else {
              if (llen<last_len) mustbeLastLine=true;
                  else if (llen>last_len) GError(ERR_FALINELEN);
          }
        }
        seqlen+=llen;
        last_len=llen;
        newSeq=false;
      } //sequence line
      //prevOffset=fl.getfpos();
    }//for each line of the fasta file
    if (seqlen>0)
      addRecord(seqname, seqlen, newSeqOffset, line_len, line_blen);
    GFREE(seqname);
    fclose(fa);
    return records.Count();
}

void GFastaIndex::addRecord(const char* seqname, uint seqlen, off_t foffs, int llen, int llen_full) {
  GFastaRec* farec=records.Find(seqname);
  if (farec!=NULL) {
       GMessage("Warning: duplicate sequence ID (%s) added to the fasta index! Only last entry data will be kept.\n");
       farec->seqlen=seqlen;
       farec->seq_offset=foffs;
       farec->line_len=llen;
       farec->line_blen=llen_full;
       }
  else {
      farec=new GFastaRec(seqlen,foffs,llen,llen_full);
      records.Add(seqname,farec);
      recTable.Add(farec);
      farec->seqname=records.getLastKey();
      }
}

int GFastaIndex::loadIndex(int set_flags) {
  if (fa_name==nullptr) GError("Error: no FASTA file name set!\n");
  clear(true, set_flags); //preserve file names
  //clunky, format could be put into flags and just use fai_load3()
  if (legacyFAI) {
    if (compressed) GError("Error: legacy FASTA index handling is not supported for compressed files!\n");
    if (fai_name==nullptr) GError("Error: no FASTA index file name set!\n");
    FILE* fi=fopen(fai_name,"rb");
    if (fi==NULL) GError(ERR_FAIDX_OPEN,fai_name);
    GLineReader fl(fi);
    char* s=NULL;
    while ((s=fl.nextLine())!=NULL) {
      if (*s=='#') continue;
      char* p=strchrs(s,"\t ");
      if (p==NULL) GError(ERR_FAIDXLINE,s);
      *p=0; //s now holds the genomic sequence name
      p++;
      long long len=0;
      int line_len=0, line_blen=0;
      long long offset=-1;
      sscanf(p, "%lld%lld%d%d", &len, &offset, &line_len, &line_blen);
      if (len==0 || line_len==0 || line_blen==0 || line_blen<line_len)
          GError(ERR_FAIDXLINE,p);
      addRecord(s,len,offset,line_len, line_blen);
    }
    fclose(fi);
    return recTable.Count();
  }
  // -- new htslib index handling
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

  // only needed for bgzf_voffs
  g_bgzidx_t* gzi = NULL;
  int last_found = 0; // Lower bound of the search space in bgzf_idx
  if (bgzf_voffs) {
     gzi = (g_bgzidx_t*) fai->bgzf->idx; // Access the internal bgzidx_t
  }
  //loop through all FASTA records in the order they were found in the file
  for (int i = 0; i < fai->n; ++i) {
      khiter_t k = kh_get(s, fai->hash, fai->name[i]);
      if (k == kh_end(fai->hash)) {
          continue;
      }
      g_faidx1_t* val = &kh_val(fai->hash, k);
      GFastaRec* rec = new GFastaRec(val->len, val->seq_offset, val->line_len, val->line_blen);
      records.Add(fai->name[i], rec);
      rec->seqname = records.getLastKey();
      recTable.Add(rec);

      if (!bgzf_voffs) continue;
      // locate virtual offset based on seq_offset
      uint64_t seq_offset=val->seq_offset;
      uint64_t caddr=0, inblock_offset=0;
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
  }
  return records.Count();
}

// returns an allocated null-terminated buffer with the requested sequence
// actual length read is returned in clen
// caller is responsible for freeing the returned char* buffer
char* GFastaIndex::fetchSeq(const char* seqname, int64_t cstart, int64_t cend, int64_t* retlen) {
  if (legacyFAI) {
    //TODO: implement un-cached sequence fetching for legacy index
    GFastaRec* val=records.Find(seqname);
    if (retlen) *retlen = -1;
    if (val==nullptr) {
      GMessage("Warning: fetchSeq could not find %s in FASTA index!\n", seqname);
      return nullptr;
    }
    if (cend<0 || cend>val->seqlen) cend=val->seqlen;
    if (cstart<1) cstart=1;
    if (cend>0 && cstart>0  && cend<cstart) Gswap(cstart, cend);

    int64_t beg=cstart-1, end=cend-1;
    off_t fofs=val->seq_offset + beg / val->line_blen * val->line_len
               + beg % val->line_blen;
    FILE* fa=fopen(fa_name,"rb");
    if (fa==NULL) GError(ERR_FASTA_OPEN, fa_name);
    int ret=fseeko(fa, fofs, SEEK_SET);
    if (ret<0) {
         GMessage("Warning: could not seek to FASTA offset %lld\n for %s", (long long)fofs,
                        seqname);
         fclose(fa);
         return nullptr;
    }
    size_t l = 0;
    char* seq;
    GMALLOC(seq, (size_t) end - beg + 2);
    int c = 0;;
    size_t maxl = (size_t)end-beg;
    while ( l < maxl && (c=fgetc(fa))>=0 )
        if (c > ' ' && c <= '~') seq[l++] = c;
    if (c < 0) {
        GMessage("Warning fetchSeq failed to retrieve chunk: %s",
            c == -1 ? "unexpected end of file" : "error reading file");
        GFREE(seq);
        return nullptr;
    }
    seq[l] = '\0';
    if (retlen) *retlen=l;
    return seq;
  }
  if (fai==nullptr) GError(GFAIDX_ERROR_NO_FAIDX);
  int64_t seqlen=faidx_seq_len64((faidx_t *)fai, seqname);
  if (seqlen<0) return nullptr;
  if (cend<0) cend=seqlen;
  int64_t len=0;
  char* seq=faidx_fetch_seq64((faidx_t*)fai, seqname, cstart-1, cend-1, &len);
  if (retlen) *retlen=len;
  return seq;
}

//legacy code
int GFastaIndex::storeIndex() { //write the hash to a file
  if (compressed) GError(ERR_FAI_LEGACY_COMPRESSED);
  if (fai_name==nullptr) GError("Error at GFastaIndex:storeIndex(): no index file name set!\n");
  if (records.Count()==0)
     GError("Error at GFastaIndex:storeIndex(): no records found!\n");
  FILE* fai=fopen(fai_name, "w");
  if (fai==NULL) GError("Error creating fasta index file: %s\n",fai_name);
  int rcount=0;
  GList<GFastaRec> reclist(true, false, true); //sorted, don't free members, unique
  records.startIterate( );
  GFastaRec* rec=NULL;
  while ((rec=records.NextData( ))!=NULL) {
    reclist.Add(rec);
  }
  //reclist has records sorted by file offset
  for (int i=0;i<reclist.Count( );i++) {
    int written=fprintf(fai, "%s\t%lld\t%lld\t%d\t%d\n",
            reclist[i]->seqname, (long long)reclist[i]->seqlen, (long long)(reclist[i]->seq_offset),
              reclist[i]->line_len, reclist[i]->line_blen);
    if (written>0) rcount++;
    else {
      GMessage("Warning: 0 bytes written to index file for record %s!\n",
               reclist[i]->seqname);
      break;
    }
  }
  fclose(fai);
  return rcount;
}
