/*
 * GFile.cpp
 *
 *  Created on: Dec 3, 2019
 *      Author: gpertea
 */

#include "GFile.h"

GFileReader::GFileReader(char* filename, int bufSize):fh(NULL),fname(NULL),fbuf(NULL),
		fbufcap(bufSize), fpos(-1), fbufpos(0), fbufzpos(0), is_eof(false) {
  GMALLOC(fbuf, fbufcap);
  if (filename!=NULL) setFile(filename);
}

void GFileReader::setFile(char* filename) {
 if (filename==NULL) GError("Error: a file name must be given!\n");
 GFREE(fname);
 fname=Gstrdup(filename);
 fh=fopen(fname, "rb");
 if (fh==NULL) GError("Error: failed opening file %s\n", fname);
 fbufzpos=0;
 fpos=0;
 fbufpos=0;
}

GFileReader::GFileReader(FILE* afh, int64_t afpos):fh(afh),fname(NULL),fbuf(NULL),
		fbufcap(16384), fpos(afpos), fbufpos(0), fbufzpos(0), is_eof(false) {
	GMALLOC(fbuf, fbufcap);
}

GFileReader::GFileReader(FILE* afh):fh(afh),fname(NULL),fbuf(NULL),
		fbufcap(16384), fpos(0), fbufpos(0), fbufzpos(0), is_eof(false)  {
	GMALLOC(fbuf, fbufcap);
	fseek(fh, 0, SEEK_SET);
}

#define GFR_CKFH if (fh==NULL) \
	GError("Error: attempt to use uninitialized file handle!\n")
#define GFR_CKBUF(ret) if (fbufzpos==0 || fbufpos==fbufzpos) {\
	freadnext(); if (is_eof) return ret; }
#define GFR_CKEOF(ret) if (is_eof) return ret


void GFileReader::freadnext() {
	//this should only be called when fbufrdpos==0 (nothing read yet)
	// or the read buffer was exhausted (fbufpos==fbufrdpos)
	if (feof(fh)) { //already end of file
		is_eof=true;
		return;
	}
	if (fbufzpos==0) { //first read -- fill the whole buffer
		int fbufread=(int)fread(fbuf, 1, fbufcap, fh);
		if (fbufread<fbufcap) {
			if (ferror(fh))
				GError("Error: failed reading block from file (%s)\n", fname);
		}
		fbufzpos=fbufread;
		fbufpos=0;
	}
	else { //subsequent reads beyond the first: preserve the last 4 bytes
		int bcap=fbufcap-4;
		//copy the last 4 bytes of the buffer at the beginning
		//memcpy((void*)fbuf, (void*)(fbuf+bcap), 4);
		*((uint32_t*)fbuf)=*((uint32_t*)(fbuf+bcap));
		//fill the rest of the buffer with new file data
		int fbufread=(int)fread((void*)(fbuf+4), 1, bcap, fh);
		if (fbufread<bcap) {
			if (ferror(fh))
				GError("Error: failed reading block from file (%s)\n", fname);
		}
		fbufpos=4;
		fbufzpos=fbufread+4;
	}
}

int GFileReader::getc() {
	//GFR_CKFH;
	GFR_CKEOF(-1);
	GFR_CKBUF(-1);
	int ch = fbuf[fbufpos];
	fbufpos++;
	fpos++;
	return ch;
}

void GFileReader::ungetc() {
  if (fpos==0 || fbufpos==0)
    GError("Error: cannot use GFileReader::ungetc() ! (fpos=%ld)\n", (long)fpos);
  fpos--;
  fbufpos--;
  is_eof=false;
}

inline size_t GFileReader::readToMem(char* mem, int64_t rcount) {
	size_t to_read=rcount;
	size_t total_read=0; //total of bytes actually read
	while (to_read>0 && !is_eof) {
		  size_t ltr=fbufzpos-fbufpos;
		  if (ltr>to_read) ltr=to_read;
		  memcpy(mem+total_read, fbuf+fbufpos, ltr);
		  to_read-=ltr;
		  fbufpos+=ltr;
		  fpos+=ltr;
		  total_read+=ltr;
		  if (to_read>0 && fbufpos==fbufzpos)
			  freadnext();
	}
	return total_read;
}

char* GFileReader::readAlloc(int64_t numBytes, int64_t* nbRead) {
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
		  size_t rbuflen=fbufzpos-fbufpos;
		  char* p = (char*) memchr(fbuf+fbufpos, (int)ch, rbuflen);
		  if (p!=NULL) {
			  int coffs=(p-fbuf)-fbufpos;
			  rlen+=coffs;
			  r.append(fbuf+fbufpos, coffs);
			  fbufpos+=coffs+1; //skip the delimiter
			  fpos+=coffs+1;
			  if (linelen!=NULL) *linelen=rlen;
			  break;
		  }
		  r.append(fbuf+fbufpos, rbuflen);
		  fbufpos+=rbuflen;
		  fpos+=rbuflen;
		  rlen+=rbuflen;
		  if (fbufpos==fbufzpos)
			  freadnext();
	}
	if (linelen!=NULL) *linelen=rlen;
	return r();
}



GFileReader::~GFileReader() {
  GFREE(fbufcap);
  GFREE(fname);
  if (fh!=NULL) fclose(fh);
}



