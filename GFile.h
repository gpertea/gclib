/*
 * GFile.h
 *
 *  Created on: Dec 3, 2019
 *      Author: gpertea
 */

#ifndef GFILE_H_
#define GFILE_H_
#include "GBase.h"

class GFileReader {
 protected:
	FILE* fh;
	char* fname;
	char* fbuf;
	int fbufcap; //buffer initial capacity
	int64_t fpos; //current reading offset in FILE* fh
	int fbufpos; //current buffer reading offset in fbuf (next byte to be consumed)
	int fbufzpos; //offset of the byte after the last loaded byte from file into fbuf
	bool is_eof; //if the last character in the file has been read
	  //by any of the get*() or read*() functions
	void freadnext(); //fetch next chunk in fbuf
    size_t readToMem(char* mem, int64_t rcount);
 public:
	GFileReader(char* filename=NULL, int bufSize=32768);
	GFileReader(FILE* afh, int64_t afpos); //passing an open file, at a known offset
	GFileReader(FILE* afh); //passing an open file (fseekpos will be unknown!)
	void setFile(char* filename);
	bool eof() { return is_eof; }
	int64_t getpos() { return fpos; }
	int getc();
	void ungetc();
	char* readAlloc(int64_t numBytes, int64_t* nbRead=NULL);
	  //read a fixed number of bytes, caller has to free the returned pointer
	size_t read(char* mem, int64_t numBytes);
	 //read numBytes from file into the pre-allocated mem block
	char* getLine(int* linelen=NULL);
	char* getUntil(const char ch, int* rlen=NULL);
	/*
	int64_t skip(int numChars); //skip numChars in the file stream, returns new file position
	GMemBlock getLine(); //caller has to deallocate the resulting GMemBlock
	GMemBlock getUntil(const char c); //caller has to mem-manage the resulting GMemBlock
	GMemBlock getUntil(const char* delim); //caller has to mem-manage the resulting GMemBlock
	*/
	~GFileReader();
};


#endif /* GFILE_H_ */
