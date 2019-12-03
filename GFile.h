#ifndef GFILE_H_
#define GFILE_H_
#include "GBase.h"

//buffered file reader
class GFileReader {
 protected:
	FILE* fh;
	char* fname;
	char* buf; //buffer
	int bufSize; //buffer capacity in bytes (buffer size)
	int64_t fpos; //current file offset (in fh) of bufpos
	int bufpos; //current buffer "reading" offset in buf
	             // (offset of next byte to be returned by get/read methods)
	int bufEnd; //buffer offset of the byte AFTER the last loaded byte from file
	             //(0 before the first loadNext(), bufSize after that
	             // unless the end of the file is reached
	bool is_eof; //if the last character in the file has been read
	  //by any of the get*() or read*() functions
	void loadNext(); //load next chunk from file into buffer
    size_t readToMem(char* mem, int64_t rcount);
 public:
	GFileReader(char* filename=NULL, int buf_size=32768);
	GFileReader(FILE* afh, int64_t afpos); //passing an open file, at a known offset
	GFileReader(FILE* afh); //passing an open file (fseekpos will be given by ftell)
	void setFile(char* filename);
	bool eof() { return is_eof; }
	int64_t getpos() { return fpos; }
	int getc();
	int ungetc(); //bufpos--, undo getc() and the last byte of any read* method
	char* read(int64_t numBytes, int64_t* nbRead=NULL);
	  //read a fixed number of bytes, caller has to free the returned pointer
	size_t read(char* mem, int64_t numBytes);
	//read numBytes from file into a pre-allocated mem block
	//
	char* getLine(int* linelen=NULL);
	char* getUntil(const char upTo, int* rlen=NULL);
	char* getUntil(const char* upTo, int* rlen=NULL);
	/*
	int64_t skip(int numChars); //skip numChars in the file stream, returns new file position
	GMemBlock getLine(); //caller has to deallocate the resulting GMemBlock
	GMemBlock getUntil(const char c); //caller has to mem-manage the resulting GMemBlock
	GMemBlock getUntil(const char* delim); //caller has to mem-manage the resulting GMemBlock
	*/
	~GFileReader();
};


#endif /* GFILE_H_ */
