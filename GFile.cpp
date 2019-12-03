#include "GFile.h"

GFileReader::GFileReader(char* filename, int buf_size):fh(NULL),fname(NULL),buf(NULL),
		bufSize(buf_size), fpos(-1), bufpos(0), bufEnd(0), is_eof(false) {
  GMALLOC(buf, bufSize);
  if (filename!=NULL) setFile(filename);
}

void GFileReader::setFile(char* filename) {
 if (filename==NULL) GError("Error: a file name must be given!\n");
 GFREE(fname);
 fname=Gstrdup(filename);
 fh=fopen(fname, "rb");
 if (fh==NULL) GError("Error: failed opening file %s\n", fname);
 bufEnd=0;
 fpos=0;
 bufpos=0;
}

GFileReader::GFileReader(FILE* afh, int64_t afpos):fh(afh),fname(NULL),buf(NULL),
		bufSize(16384), fpos(afpos), bufpos(0), bufEnd(0), is_eof(false) {
	GMALLOC(buf, bufSize);
}

GFileReader::GFileReader(FILE* afh):fh(afh),fname(NULL),buf(NULL),
		bufSize(16384), fpos(0), bufpos(0), bufEnd(0), is_eof(false)  {
	GMALLOC(buf, bufSize);
	fseek(fh, 0, SEEK_SET);
}

#define GFR_CKFH if (fh==NULL) \
	GError("Error: attempt to use uninitialized file handle!\n")
#define GFR_CKBUF(ret) if (bufEnd==0 || bufpos==bufEnd) {\
	                        loadNext(); if (is_eof) return ret; }
#define GFR_CKEOF(ret) if (is_eof) return ret


void GFileReader::loadNext() {
	//this should only be called when fbufrdpos==0 (nothing read yet)
	// or the read buffer was exhausted (fbufpos==fbufrdpos)
	if (feof(fh)) { //already end of file
		is_eof=true;
		return;
	}
	if (bufEnd==0) { //first read -- fill the whole buffer
		int fbufread=(int)fread(buf, 1, bufSize, fh);
		if (fbufread<bufSize) {
			if (ferror(fh))
				GError("Error: failed reading block from file (%s)\n", fname);
		}
		bufEnd=fbufread;
		bufpos=0;
	}
	else { //subsequent reads beyond the first: preserve the last 4 bytes
		int bcap=bufSize-4;
		//copy the last 4 bytes of the buffer at the beginning
		//memcpy((void*)fbuf, (void*)(fbuf+bcap), 4);
		*((uint32_t*)buf)=*((uint32_t*)(buf+bcap));
		//fill the rest of the buffer with new file data
		int fbufread=(int)fread((void*)(buf+4), 1, bcap, fh);
		if (fbufread<bcap) {
			if (ferror(fh))
				GError("Error: failed reading block from file (%s)\n", fname);
		}
		bufpos=4;
		bufEnd=fbufread+4;
	}
}

int GFileReader::getc() {
	//GFR_CKFH;
	GFR_CKEOF(-1);
	GFR_CKBUF(-1);
	int ch = buf[bufpos];
	bufpos++;
	fpos++;
	return ch;
}

int GFileReader::ungetc() {
  if (fpos==0 || bufpos==0)
    //GError("Error: cannot use GFileReader::ungetc() ! (fpos=%ld)\n", (long)fpos);
	return -1;
  fpos--;
  bufpos--;
  int ch = buf[bufpos];
  is_eof=false;
  return ch;
}

inline size_t GFileReader::readToMem(char* mem, int64_t rcount) {
	size_t to_read=rcount;
	size_t total_read=0; //total of bytes actually read
	while (to_read>0 && !is_eof) {
		  size_t ltr=bufEnd-bufpos;
		  if (ltr>to_read) ltr=to_read;
		  memcpy(mem+total_read, buf+bufpos, ltr);
		  to_read-=ltr;
		  bufpos+=ltr;
		  fpos+=ltr;
		  total_read+=ltr;
		  if (to_read>0 && bufpos==bufEnd)
			  loadNext();
	}
	return total_read;
}

char* GFileReader::read(int64_t numBytes, int64_t* nbRead) {
  char* r=NULL;
  GFR_CKFH;
  GFR_CKEOF(r);
  GFR_CKBUF(r);
  GMALLOC(r, numBytes);
  size_t actual_read=readToMem(r, numBytes);
  if (nbRead!=NULL) *nbRead=(int64_t) actual_read;
  return r;
}

size_t GFileReader::read(char* mem, int64_t numBytes) {
	GFR_CKFH;
	GFR_CKEOF(0);
	GFR_CKBUF(0);
	size_t actual_read=readToMem(mem, numBytes);
	return actual_read;
}

char* GFileReader::getLine(int* linelen) {
	GFR_CKFH;
	GFR_CKEOF(NULL);
	GFR_CKBUF(NULL);
	int rlen=0;
	GDynArray<char> r(512); //result buffer
	while (!is_eof) {
       int c=this->getc();
       if (c<0) break;
       if (c=='\n' || c=='\r') {
    	   int b=this->getc();
    	   if (b==c || (b!='\n' && b!='\r' && b>=0)) {
    		   this->ungetc();
    	   }
    	   break;
       }
       rlen++;
       r.Add((char)c);
	}
	r.zPack('\0');
	r.ForgetPtr();
	if (linelen!=NULL) *linelen=rlen;
	return r();
}

char* GFileReader::getUntil(const char ch, int* linelen) {
	GFR_CKFH;
	GFR_CKEOF(NULL);
	GFR_CKBUF(NULL);
	int rlen=0;
	GDynArray<char> r(512); //result buffer
	while (!is_eof) {
		  //scan for ch from fbuf+fbufpos to fbuf+fbufread-1
		  size_t rbuflen=bufEnd-bufpos;
		  char* p = (char*) memchr(buf+bufpos, (int)ch, rbuflen);
		  if (p!=NULL) {
			  int coffs=(p-buf)-bufpos;
			  rlen+=coffs;
			  r.append(buf+bufpos, coffs);
			  bufpos+=coffs+1; //skip the delimiter
			  fpos+=coffs+1;
			  if (linelen!=NULL) *linelen=rlen;
			  break;
		  }
		  r.append(buf+bufpos, rbuflen);
		  bufpos+=rbuflen;
		  fpos+=rbuflen;
		  rlen+=rbuflen;
		  if (bufpos==bufEnd)
			  loadNext();
	}
	if (linelen!=NULL) *linelen=rlen;
	return r();
}



GFileReader::~GFileReader() {
  GFREE(bufSize);
  GFREE(fname);
  if (fh!=NULL) fclose(fh);
}



