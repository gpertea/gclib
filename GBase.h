#ifndef G_BASE_DEFINED
#define G_BASE_DEFINED
#define GCLIB_VERSION "0.12.7"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(__WIN32) || defined(__WIN32__) || defined(_WIN32) || defined(_WIN64) || defined(__MINGW64__) || defined(__WINDOWS__)
  #ifndef _WIN32
    #define _WIN32
  #endif
  #ifndef _WIN64
    #define _WIN64
  #endif
  #define __USE_MINGW_ANSI_STDIO 1
  //#define __ISO_C_VISIBLE 1999
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
  #define CHPATHSEP '\\'
  #undef off_t
  #define off_t int64_t
  #ifndef popen
   #define popen _popen
  #endif
  /*
  #ifndef fseeko
		#ifdef _fseeki64
			#define fseeko(stream, offset, origin) _fseeki64(stream, offset, origin)
		#else
			#define fseeko fseek
		#endif
  #endif
  #ifndef ftello
    #ifdef _ftelli64
      #define ftello(stream) _ftelli64(stream)
    #else
      #define ftello ftell
    #endif
 #endif
 */
 #else
  #define CHPATHSEP '/'
  #ifdef __CYGWIN__
    #define _BSD_SOURCE
  #endif
  #include <unistd.h>
#endif

#ifdef DEBUG
#undef NDEBUG
#define _DEBUG 1
#define _DEBUG_ 1
#endif

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <math.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdint>
#include <cstdarg>
#include <cctype>
#include <type_traits>

typedef int64_t int64;
typedef uint64_t uint64;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int16_t int16;
typedef uint16_t uint16;

typedef unsigned char uchar;
typedef uint8_t byte;
typedef unsigned int uint;

typedef void* pointer;


#ifndef MAXUINT
#define MAXUINT ((unsigned int)-1)
#endif

#ifndef MAXINT
#define MAXINT INT_MAX
#endif

#ifndef MAX_UINT
#define MAX_UINT ((unsigned int)-1)
#endif

#ifndef MAX_INT
#define MAX_INT INT_MAX
#endif

/****************************************************************************/

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

/****************************************************************************/
#define ERR_ALLOC "Error allocating memory.\n"

//-------------------

#define GEXIT(a) { \
fprintf(stderr, "Error: "); fprintf(stderr, a); \
GError("Exiting from line %i in file %s\n",__LINE__,__FILE__); \
}

// Debug helpers
#ifndef NDEBUG
 #define GASSERT(exp) ((exp)?((void)0):(void)GAssert(#exp,__FILE__,__LINE__))
 #define GVERIFY(condition) \
if (!(condition)) { \
fprintf(stderr, "Assumption \"%s\"\nFailed in file %s: at line:%i\n", \
#condition,__FILE__,__LINE__); \
GEXIT(#condition);}
 #ifdef TRACE
  #define GTRACE(exp)  (GMessage(exp))
 #else
  #define GTRACE(exp)
 #endif
#else
 #define GASSERT(exp)
 #define GTRACE(exp)
 #define GVERIFY(condition)
#endif

#define GERROR(exp) (GError(exp))

// Abolute value
#define GABS(val) (((val)>=0)?(val):-(val))

// Min and Max
#define GMAX(a,b) (((a)>(b))?(a):(b))
#define GMIN(a,b) (((a)>(b))?(b):(a))

// Min of three
#define GMIN3(x,y,z) ((x)<(y)?GMIN(x,z):GMIN(y,z))

// Max of three
#define GMAX3(x,y,z) ((x)>(y)?GMAX(x,z):GMAX(y,z))

// Return minimum and maximum of a, b
#define GMINMAX(lo,hi,a,b) ((a)<(b)?((lo)=(a),(hi)=(b)):((lo)=(b),(hi)=(a)))

// Clamp value x to range [lo..hi]
#define GCLAMP(lo,x,hi) ((x)<(lo)?(lo):((x)>(hi)?(hi):(x)))

typedef int GCompareProc(const pointer item1, const pointer item2);
typedef long GFStoreProc(const pointer item1, FILE* fstorage); //for serialization
typedef pointer GFLoadProc(FILE* fstorage); //for deserialization

void GError(const char* format,...); // Error routine (aborts program)
void GMessage(const char* format,...);// Log message to stderr
// Assert failed routine:- usually not called directly but through GASSERT
void GAssert(const char* expression, const char* filename, unsigned int lineno);

typedef void GFreeProc(pointer item); //usually just delete,
      //but may also support structures with embedded dynamic members

#define GMALLOC(ptr,size)  if (!GMalloc((pointer*)(&ptr),size)) \
                                     GError(ERR_ALLOC)

#define GCALLOC(ptr,size)  if (!GCalloc((pointer*)(&ptr),size)) \
                                     GError(ERR_ALLOC)
#define GREALLOC(ptr,size) if (!GRealloc((pointer*)(&ptr),size)) \
                                     GError(ERR_ALLOC)
#define GFREE(ptr)       GFree((pointer*)(&ptr))

inline char* strMin(char *arg1, char *arg2) {
    return (strcmp(arg1, arg2) < 0)? arg1 : arg2;
}

inline char* strMax(char *arg1, char *arg2) {
    return (strcmp(arg2, arg1) < 0)? arg1 : arg2;
}

inline int iround(double x) {
   return (int)floor(x + 0.5);
}

char* Grealpath(const char *path, char *resolved_path);

int Gmkdir(const char *path, bool recursive=true, int perms = (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH));

void Gmktempdir(char* templ);

bool haveStdInput(); //if stdin is from a pipe or redirection

/****************************************************************************/

static inline int Gintcmp(int a, int b) {
 //return (a>b)? 1 : ((a==b)?0:-1);
  return a-b;
}

static inline int Gstrlen(const char* a) {
  return ((a==NULL) ? 0 : strlen(a));
}

int Gstrcmp(const char* a, const char* b, int n=-1);
//same as strcmp but doesn't crash on NULL pointers

int Gstricmp(const char* a, const char* b, int n=-1);
bool GstrEq(const char* a, const char* b);
bool GstriEq(const char* a, const char* b);

//basic swap template function
template<class T> void Gswap(T& lhs, T& rhs) {
 T tmp=lhs; //requires copy operator
 lhs=rhs;
 rhs=tmp;
}

// use std::is_pointer from <type_traits> in C++11 instead
/*
template<typename T>
  struct isPointer { static const bool value = false; };

template<typename T>
  struct isPointer<T*> { static const bool value = true; };
*/
//check if type T is resolved as a pointer to char
template<class T>
  struct is_char_ptr : std::integral_constant <
      bool,
      std::is_same<char const *, typename std::decay<T>::type>::value ||
        std::is_same<char *, typename std::decay<T>::type>::value
  > {};



inline void GFree(pointer* ptr){
     GASSERT(ptr);
     if (*ptr) free(*ptr);
     *ptr=NULL;
 }

inline bool GMalloc(pointer* ptr,unsigned long size){
    //GASSERT(ptr);
    if (size!=0)
  	  *ptr=malloc(size);
    return *ptr!=NULL;
 }

// Allocate 0-filled memory
inline bool GCalloc(pointer* ptr,unsigned long size){
    GASSERT(ptr);
    *ptr=calloc(size,1);
    return *ptr!=NULL;
 }

// Resize memory
inline bool GRealloc(pointer* ptr,unsigned long size){
    //GASSERT(ptr);
    if (size==0) {
      GFree(ptr);
      return true;
      }
    if (*ptr==NULL) {//simple malloc
     void *p=malloc(size);
     if (p != NULL) {
       *ptr=p;
       return true;
       }
      else return false;
     }//malloc
    else {//realloc
     void *p=realloc(*ptr,size);
     if (p) {
         *ptr=p;
         return true;
         }
     return false;
     }
 }

template<class T> T* GDupAlloc(T& data) {
	T* tmp=NULL;
	if (!GMalloc((pointer*) tmp, sizeof(T)))
			GError(ERR_ALLOC);
	memcpy((void*)tmp, (void*)&data, sizeof(T));
	return tmp;
}

// ****************** basic string manipulation *************************
char *Gstrdup(const char* str, int xtracap=0); //string duplication with extra capacity added
//duplicate a string by allocating a copy for it (+xtracap heap room) and returning the new pointer
//caller is responsible for deallocating the returned pointer!

char* Gstrdup(const char* sfrom, const char* sto);
//same as GStrdup, but with an early termination (e.g. on delimiter)

char* Gsubstr(const char* str, char* from, char* to=NULL);
//extracts a substring, allocating it, including boundaries (from/to)

char* replaceStr(char* &str, char* newvalue);

//conversion: to Lower/Upper case
// creating a new string:
char* upCase(const char* str);
char* loCase(const char* str);
// changing string in place:
char* strlower(char * str);
char* strupper(char * str);


//strstr but for memory zones: scans a memory region
//for a substring:
void* Gmemscan(void *mem, unsigned int len,
                  void *part, unsigned int partlen);

FILE* Gfopen(const char *path, char *mode=NULL);

// test if a char is in a string:
bool chrInStr(char c, const char* str);

char* rstrchr(char* str, char ch);
/* returns a pointer to the rightmost
  occurence of ch in str - like rindex for platforms missing it*/

char* strchrs(const char* s, const char* chrs);
//strchr but with a set of chars instead of only one

char* rstrfind(const char* str, const char *substr);
// like rindex() but for strings;  right side version of strstr()

char* reverseChars(char* str, int slen=0); //in place reversal of string

char* rstrstr(const char* rstart, const char *lend, const char* substr);
/*the reversed, rightside equivalent of strstr: starts searching
 from right end (rstart), going back to left end (lend) and returns
 a pointer to the last (right) matching character in str */

char* strifind(const char* str,  const char* substr);
// case insensitive version of strstr -- finding a string within another string
// returns NULL if not found

//Determines if a string begins with a given prefix
//(returns false when any of the params is NULL,
// but true when prefix is '' (empty string)!)
bool startsWith(const char* s, const char* prefix);

bool startsiWith(const char* s, const char* prefix); //case insensitive


bool endsWith(const char* s, const char* suffix);
//Note: returns true if suffix is empty string, but false if it's NULL
bool endsiWith(const char* s, const char* suffix); //case insensitive version

//like endsWith but also remove the suffix if found
//returns true if the given suffix was found and removed
bool trimSuffix(char* s, const char* suffix);
//case insensitive version:
bool trimiSuffix(char* s, const char* suffix);

// ELF hash function for strings
int strhash(const char* str);

//alternate hash functions:
int fnv1a_hash(const char* cp);
int djb_hash(const char* cp);

//---- generic base GSeg : genomic segment (interval) --
// coordinates are considered 1-based (so 0 is invalid)
struct GSeg {
  uint start; //start<end always!
  uint end;
  GSeg(uint s=0,uint e=0) {
    if (s>e) { start=e;end=s; }
        else { start=s;end=e; }
  }
  //check for overlap with other segment
  uint len() { return end-start+1; }
  bool overlap(GSeg* d) {
     return (start<=d->end && end>=d->start);
  }

  bool overlap(GSeg& d) {
     return (start<=d.end && end>=d.start);
  }

  bool overlap(GSeg& d, int fuzz) {
     return (start<=d.end+fuzz && end+fuzz>=d.start);
  }

  bool overlap(uint x) {
	return (start<=x && x<=end);
  }

  bool overlap(uint s, uint e) {
     if (s>e) { Gswap(s,e); }
     return (start<=e && end>=s);
  }

  //return the length of overlap between two segments
  int overlapLen(GSeg* r) {
     if (start<r->start) {
        if (r->start>end) return 0;
        return (r->end>end) ? end-r->start+1 : r->end-r->start+1;
        }
       else { //r->start<=start
        if (start>r->end) return 0;
        return (r->end<end)? r->end-start+1 : end-start+1;
        }
     }
  int overlapLen(uint rstart, uint rend) {
     if (rstart>rend) { Gswap(rstart,rend); }
     if (start<rstart) {
        if (rstart>end) return 0;
        return (rend>end) ? end-rstart+1 : rend-rstart+1;
        }
       else { //rstart<=start
        if (start>rend) return 0;
        return (rend<end)? rend-start+1 : end-start+1;
        }
  }

  bool contains(GSeg* s) {
	  return (start<=s->start && end>=s->end);
  }
  bool contained(GSeg* s) {
	  return (s->start<=start && s->end>=end);
  }

  bool equals(GSeg& d){
      return (start==d.start && end==d.end);
  }
  bool equals(GSeg* d){
      return (start==d->start && end==d->end);
  }

  //fuzzy coordinate matching:
  bool coordMatch(GSeg* s, uint fuzz=0) { //caller must check for s!=NULL
    if (fuzz==0) return (start==s->start && end==s->end);
    uint sd = (start>s->start) ? start-s->start : s->start-start;
    uint ed = (end>s->end) ? end-s->end : s->end-end;
    return (sd<=fuzz && ed<=fuzz);
  }
  void expand(int by) { //expand in both directions
	  start-=by;
	  end+=by;
  }
  void expandInclude(uint rstart, uint rend) { //expand to include given coordinates
	 if (rstart>rend) { Gswap(rstart,rend); }
	 if (rstart<start) start=rstart;
	 if (rend>end) end=rend;
  }
  //comparison operators required for sorting
  bool operator==(GSeg& d){
      return (start==d.start && end==d.end);
      }
  bool operator<(GSeg& d){
     return (start==d.start)?(end<d.end):(start<d.start);
     }
};

struct GRangeParser: GSeg {
	char* refName=NULL;
	char strand=0;
	void parse(char* s);
	GRangeParser(char* s=NULL):GSeg(0, 0) {
		if (s) parse(s);
	}
	~GRangeParser() {
		GFREE(refName);
	}
};

//basic dynamic array template for primitive types
//which can only grow (reallocate) as needed

//optimize index test
#define GDynArray_INDEX_ERR "Error: use of index (%d) in GDynArray of size %d!\n"
 #if defined(NDEBUG) || defined(NODEBUG) || defined(_NDEBUG) || defined(NO_DEBUG)
 #define GDynArray_TEST_INDEX(x)
#else
 #define GDynArray_TEST_INDEX(x) \
 if (fCount==0 || x>=fCount) GError(GDynArray_INDEX_ERR, x, fCount)
#endif

#define GDynArray_MAXCOUNT UINT_MAX-1
#define GDynArray_NOIDX UINT_MAX

//basic dynamic array (vector) template for primitive types or structs
//Note: uses malloc so it will never call the item's STYPE constructor when growing
//                                    nor the STYPE destructor

template<class STYPE> class GDynArray {
 protected:
    bool byptr=false; //if true, this is in-place pointer takeover of an existing STYPE[]
    STYPE *fArray=NULL;
    uint fCount=0;
    uint fCapacity=0; // size of allocated memory
    const static uint dyn_array_defcap = 16; // initial capacity (in elements)
    constexpr static const char* ERR_MAX_CAPACITY = "Error at GDynArray: max capacity reached!\n";
    inline void add(STYPE item) {
    	 //if (fCount==MAX_UINT-1) GError(ERR_MAX_CAPACITY);
    	 if ((++fCount) > fCapacity) Grow();
    	 fArray[fCount-1] = item;
    }
 public:
    GDynArray(int initcap=dyn_array_defcap):byptr(false), fArray(NULL), fCount(0),
	     fCapacity(initcap) { // constructor
    	  GMALLOC(fArray, fCapacity*sizeof(STYPE));
    }

    GDynArray(const GDynArray &a):byptr(false), fArray(NULL),
    		fCount(a.fCount), fCapacity(a.fCapacity) { // deep copy constructor
        GMALLOC(fArray, sizeof(STYPE)*a.fCapacity);
        memcpy(fArray, a.fArray, sizeof(STYPE)* a.fCapacity);
    }
    GDynArray(STYPE* ptr, uint pcap):byptr(true), fArray(ptr), fCount(0), fCapacity(pcap) {
    	//this will never deallocate the passed pointer
    }

    virtual ~GDynArray() { if (!byptr) { GFREE(fArray); } }

    GDynArray& operator = (const GDynArray &a) { // assignment operator
    	                                    //deep copy
        if (this == &a) return *this;
    	if (a.fCount == 0) {
    		Clear();
    		return *this;
    	}
    	growTo(a.fCapacity); //set size
        memcpy(fArray, a.fArray, sizeof(STYPE)*a.fCount);
        return *this;
    }

    STYPE& operator[] (uint idx) {// get array item
    	GDynArray_TEST_INDEX(idx);
    	return fArray[idx];
    }

    inline void Grow() {
    	int delta = (fCapacity>16) ? (fCapacity>>2) : 2;
    	if (GDynArray_MAXCOUNT-delta<=fCapacity)
    		delta=GDynArray_MAXCOUNT-fCapacity;
    	if (delta<=1) GError(ERR_MAX_CAPACITY);
    	growTo(fCapacity + delta);
    }

    uint Add(STYPE* item) { // Add item to the end of array
      //element given by pointer
      if (item==NULL) return GDynArray_NOIDX;
  	  this->add(*item);
  	  return (fCount-1);
    }

    STYPE& Add() {
   	 if (fCount==MAX_UINT-1) GError(ERR_MAX_CAPACITY);
   	 if ((++fCount) > fCapacity) Grow();
   	 return fArray[fCount-1];
    }

    inline uint Add(STYPE item) { // Add STYPE copy to the end of array
	    this->add(item);
	    return (fCount-1);
    }

    uint Push(STYPE item) { //same as Add
    	 this->add(item);
    	 return (fCount-1);
    }

    STYPE Pop() { //shoddy.. Do NOT call this for an empty array!
    	if (fCount==0) return (STYPE)NULL; //a NULL cast operator is required
    	--fCount;
    	return fArray[fCount];
    }

    uint Count() { return fCount; } // get size of array (elements)
    uint Capacity() { return fCapacity; }
    inline void growTo(uint newcap) {
    	if (newcap==0) { Clear(); return; }
    	if (newcap <= fCapacity) return; //never shrink! (use Pack() for shrinking)
    	GREALLOC(fArray, newcap*sizeof(STYPE));
    	fCapacity=newcap;
    }

    void append(const STYPE* arr, uint count=0) {
    	//fast adding of a series of objects
      if (count>0) {
    	  growTo(fCount+count);
    	  memcpy(fArray+fCount, arr, count*sizeof(STYPE));
    	  fCount+=count;
      }
    }

    void append(GDynArray<STYPE> arr) {
    	//fast adding of a series of objects
    	growTo(fCount+arr.fCount);
    	memcpy(fArray+fCount, arr.fArray, arr.fCount*sizeof(STYPE));
    	fCount+=arr.fCount;
    }

    void Trim(int tcount=1) {
    	//simply cut (discard) the last tcount items
    	//new Count is now fCount-tcount
    	//does NOT shrink capacity accordingly!
    	if (fCount>=tcount) fCount-=tcount;
    }

    void Pack() { //shrink capacity to fCount+dyn_array_defcap
    	if (fCapacity-fCount<=dyn_array_defcap) return;
    	int newcap=fCount+dyn_array_defcap;
    	GREALLOC(fArray, newcap*sizeof(STYPE));
    	fCapacity=newcap;
    }

    void zPack(STYPE z) { //shrink capacity to fCount+1 and adds a z terminator
    	if (fCapacity-fCount<=1) { fArray[fCount]=z; return; }
    	int newcap=fCount+1;
    	GREALLOC(fArray, newcap*sizeof(STYPE));
    	fCapacity=newcap;
    	fArray[fCount]=z;
    }

    inline void Shrink() { Pack(); }
    void Clear() { // clear array, shrinking its allocated memory
    	fCount = 0;
    	GFREE(fArray);
    	//GREALLOC(fArray, sizeof(STYPE)*dyn_array_defcap);
    	GMALLOC(fArray, sizeof(STYPE)*dyn_array_defcap);
    	// set initial memory size again
    	fCapacity = dyn_array_defcap;
    }

    void Reset() {// fast clear array WITHOUT deallocating it
    	fCount = 0;
    }

    void Delete(uint idx) {
	    GDynArray_TEST_INDEX(idx);
	    --fCount;
	    if (idx<fCount)
		    memmove(&fArray[idx], &fArray[idx+1], (fCount-idx)*sizeof(STYPE));
    }

    inline void Remove(uint idx) { Delete(idx); }

    // Warning: not const, the returned pointer (array) can be modified
    STYPE* operator()() { return fArray; }

    //use methods below in order to prevent deallocation of fArray pointer on destruct
    //could be handy for adopting stack objects (e.g. cheap dynamic strings)
    void ForgetPtr() { byptr=true;  }
    void DetachPtr() { byptr=true;  }

};

int strsplit(char* str, GDynArray<char*>& fields, const char* delim, int maxfields=MAX_INT);
//splits a string by placing 0 where any of the delim chars are found, setting fields[] to the beginning
//of each field (stopping after maxfields); returns number of fields parsed

int strsplit(char* str, GDynArray<char*>& fields, const char delim, int maxfields=MAX_INT);
//splits a string by placing 0 where the delim char is found, setting fields[] to the beginning
//of each field (stopping after maxfields); returns number of fields parsed

int strsplit(char* str, GDynArray<char*>& fields, int maxfields=MAX_INT); //splits by tab or space
//splits a string by placing 0 where tab or space is found, setting fields[] to the beginning
//of each field (stopping after maxfields); returns number of fields parsed


// Basic dynamic C string (0-terminated character array)
// Note: this always initializes as a string of length 0, so fCount is always >0
class Gcstr: public GDynArray<char> {

  friend bool operator==(const char* s1, const Gcstr& s2) {
    if (s1==NULL) return s2.is_empty();
    return (strcmp(s1, s2.chars()) == 0);
  }

  friend bool operator<(const char* s1, const Gcstr& s2) {
    if (s1==NULL) return !s2.is_empty();
    return (strcmp(s1, s2.chars()) < 0);
  }

  friend bool operator<=(const char* s1, const Gcstr& s2) {
    if (s1==NULL) return true;
    return (strcmp(s1, s2.chars()) <= 0);
  }

  friend bool operator>(const char* s1, const Gcstr& s2) {
    if (s1==NULL) return false;
    return (strcmp(s1, s2.chars()) > 0);
  }

  friend bool operator>=(const char* s1, const Gcstr& s2) {
    if (s1==NULL) return true;
    return (strcmp(s1, s2.chars()) >= 0);
  }

  friend bool operator!=(const char* s1, const Gcstr& s2) {
    if (s1==NULL) return true;
    return (strcmp(s1, s2.chars()) != 0);
  }

  public:
  Gcstr(int initcap=dyn_array_defcap):GDynArray<char>(initcap?initcap:4) {
     this->Reset();
  }

  Gcstr(const char* s, int extra_room=4):
    GDynArray<char>(Gstrlen(s)+extra_room+1) {
      this->append(s);
  }

  Gcstr(const Gcstr& s, int extra_room=4):
    GDynArray<char>(s.length()+extra_room+1) {
	  this->Reset();
      this->append_buf(s.chars(), s.length());
  }


  Gcstr(int num_chars, const char* s):GDynArray<char>(num_chars+1) {
     if (num_chars==0) return;
     this->append(s, num_chars);
  }

  Gcstr& operator=(const Gcstr& s) { Reset(); growTo(s.length()); append_buf(s, s.length()); return *this; }

  Gcstr& operator=(const char* s) {  Reset();  int slen=Gstrlen(s);  growTo(slen);  append_buf(s, slen);  return *this; }

  inline int length() const { return ((fCount>0) ? fCount-1 : 0); }

  inline int len() const { return ((fCount>0) ? fCount-1 : 0); }

  const char* chars() const {  return fArray;  }

  operator const char* () const { return fArray; }

  void Clear(int cap=0) { // clear array, shrinking its allocated memory
	  GFREE(fArray);
	  if (cap==0) cap=dyn_array_defcap;
	  GMALLOC(fArray, cap);
      // set initial memory size again
      fCapacity = cap;
      this->Reset();
  }

  inline void Reset() {// fast clear array WITHOUT deallocating it
   	fCount = 1;
    fArray[0]='\0';
  }

  Gcstr& append(const char* s, uint count=0) {
     uint len=Gstrlen(s);
     //must have \0 as the last element!
     if (fCount==0) this->Add('\0');
     GASSERT(fArray[fCount-1]==0);
     if (len>0) {
        if (count>0) { len=GMIN(len, count); }
        growTo(fCount+len);
        memcpy(fArray+fCount-1, s, len);
        fCount+=len;
        fArray[fCount-1]=0;
     }
     return *this;
  }

  inline bool is_empty() const { return fCount<=1; }

  //same as above but requires a fixed count (faster)
  Gcstr& append_buf(const char* b, int count) {
     if (count<=0) return *this;
     growTo(fCount+count);
     memcpy(fArray+fCount-1, b, count);
     fCount+=count;
     fArray[fCount-1]=0; //to be consistent
     return *this;
  }

  Gcstr& append(const Gcstr& s) { return append_buf( s.chars(), s.length() ); }

  Gcstr& append(const char* s, int from, int to=0) {
     int len=Gstrlen(s);
     //must have \0 as the last element!
     if (fCount==0) this->Add('\0');
     GASSERT(fArray[fCount-1]==0);
     if (to<=0) to=len-1-to;
     if (len>0 && from<len && to<len && to>=from) {
        int cplen=to-from+1;
        growTo(fCount+cplen);
        memcpy(fArray+fCount-1, s+from, cplen);
        fCount+=cplen;
        fArray[fCount-1]=0;
     }
     return *this;
  }
  Gcstr& append(char c) {
    growTo(fCount+1);
    fArray[fCount-1]=c;
    fArray[fCount]='\0';
    fCount++;
    return *this;
  }

  Gcstr& append(int i) {
    char buf[20];
    sprintf(buf,"%d",i);
    return append((const char*) buf);
  }

  Gcstr& append(uint i) {
    char buf[20];
    sprintf(buf,"%u",i);
    return append((const char*)buf);
  }

  Gcstr& append(long l) {
    char buf[20];
    sprintf(buf,"%ld",l);
    return append((const char*)buf);
  }

  Gcstr& append(unsigned long l) {
    char buf[20];
    sprintf(buf,"%lu", l);
    return append((const char*)buf);
  }

  Gcstr& append(double f) {
    char buf[30];
    sprintf(buf,"%f",f);
    return append((const char*)buf);
  }

  void trim(uint tcount=1) {
   	//cut (discard) the last tcount non-zero characters
   	//new Count is now fCount-tcount
   	//does NOT shrink allocated capacity!
    if (fCount==0) { this->Add('\0'); return; }
    fCount--; //string length
    if (tcount>fCount) tcount=fCount;
   	fCount-=tcount;
    fArray[fCount]='\0';
    fCount++;
  }

  inline char last() {
    if (fCount==0)  this->Add('\0');
    //for empty strings returns 0
    return ( (fCount>1) ? fArray[fCount-2] : 0 );
  }
 //trim endch or any new line characters
  void chomp(char endch=0) {
    if (fCount==0) { this->Add('\0'); return; }
    //trim line endings - default is \n and \r
    uint slen=fCount-1;
    if (endch) {
      while (slen>0 && fArray[slen-1]==endch) slen--;
    } else {
      while (slen>0 && (fArray[slen-1]=='\n' || fArray[slen-1]=='\r')) slen--;
    }
    if (slen<fCount-1) { fArray[slen]='\0'; fCount=slen; }
  }

  bool operator==(const Gcstr& s) const {
    if (s.is_empty()) return is_empty();
    return (length() == s.length()) &&
      (strcmp(chars(), s.chars()) == 0);
  }

  bool operator==(const char *s) const {
    if (s==NULL) return is_empty();
    return (strcmp(chars(), s) == 0);
  }

  bool operator<(const Gcstr& s) const {
    if (s.is_empty()) return false;
    return (strcmp(chars(), s.chars()) < 0);
  }

  bool operator<(const char *s) const {
    if (s==NULL) return false;
    return (strcmp(chars(), s) < 0);
  }

  bool operator<=(const Gcstr& s) const {
    if (s.is_empty()) return is_empty();
    return (strcmp(chars(), s.chars()) <= 0);
  }

  bool operator<=(const char *s) const {
    if (s==NULL) return is_empty();
    return (strcmp(chars(), s) <= 0);
  }

  bool operator>(const Gcstr& s) const {
    if (s.is_empty()) return !is_empty();
    return (strcmp(chars(), s.chars()) > 0);
  }

  bool operator>(const char *s) const {
    if (s==NULL) return !is_empty();
    return (strcmp(chars(), s) > 0);
  }

  bool operator>=(const Gcstr& s) const {
    if (s.is_empty()) return true;
    return (strcmp(chars(), s.chars()) >= 0);
  }

  bool operator>=(const char *s) const {
    if (s==NULL) return true;
    return (strcmp(chars(), s) >= 0);
  }

  bool operator!=(const Gcstr& s) const {
    if (s.is_empty()) return !is_empty();
    return (length() != s.length()) ||
          (memcmp(chars(), s.chars(), length()) != 0);
    }

  bool operator!=(const char *s) const {
    if (s==NULL) return !is_empty();
    return (strcmp(chars(), s) != 0);
  }

  Gcstr& operator+=(const Gcstr& s) { return append_buf(s.chars(), s.length() ); }
  Gcstr& operator+=(const char* s) { return append(s); }
  Gcstr& operator+=(char c) { return append(c); }
  Gcstr& operator+=(int i) { return append(i); }
  Gcstr& operator+=(uint i) { return append(i); }
  Gcstr& operator+=(long l) { return append(l); }
  Gcstr& operator+=(unsigned long l) { return append(l); }
  Gcstr& operator+=(double f);

  int asInt(int base = 10 ) {  return strtol(chars(), NULL, base); }

  double asReal() { return strtod( chars(), NULL); }

  inline Gcstr& upper() {
    for (char *p = fArray; *p; p++) *p = (char) toupper(*p);
    return *this;
  }

  inline Gcstr& lower() {
    for (char *p = fArray; *p; p++) *p = (char) tolower(*p);
    return *this;
  }

  int index(const char *s, int start_index=0) const {
    // A negative index specifies an index from the right of the string.
    if (s==NULL || s[0]==0 ) return -1;
    if (start_index < 0) start_index += length();
    if (start_index < 0 || start_index >= length()) return -1;
    const char* idx = strstr(&fArray[start_index], s);
    return idx ? idx-fArray : -1;
}

 int index(char c, int start_index=0 ) const {
    // A negative index specifies an index from the right of the string.
    if (length()==0 || c=='\0') return -1;
    if (start_index < 0) start_index += length();
    if (start_index < 0 || start_index >= length()) return -1;
    const char *idx=(char *) ::memchr(&fArray[start_index], c,
                                         length()-start_index);
    return idx ? idx - fArray : -1;
}

int rindex(char c, int end_index=-1) const {
    if (c == 0 || length()==0 || end_index>=length()) return -1;
    if (end_index<0) end_index=length()-1;
    for (int i=end_index;i>=0;i--)  if (fArray[i]==c) return i;
    return -1;
}

int rindex(const char* str, int end_index=-1) const {
    if (str==NULL || *str == '\0' || length()==0 || end_index>=length())
        return -1;
    int slen=strlen(str);
    if (end_index<0) end_index=length()-1;
    //end_index is the index of the right-side boundary
    //the scanning starts at the end
    if (end_index>=0 && end_index<slen-1) return -1;
    for (int i=end_index-slen+1;i>=0;i--) {
       if (memcmp((void*)(fArray+i), (void*)str, slen)==0)
           return i;
       }
    return -1;
}

Gcstr&  cut(int idx) {
    //cut off from index idx to the end of string
    //negative index means from the end of the string
    if (idx < 0)  idx += length();
    if (idx<0 || (uint)idx>=fCount-1) return *this; //invalid index
    fCount=idx+1;
    fArray[idx]='\0';
    return *this;
}

//delete string span without reallocation
Gcstr& remove(int idx, int rlen=-1) {
	    if (rlen==-1) return cut(idx);
      if (idx < 0)  idx += length();
      if (rlen==0 || idx<0 || (uint)idx>=fCount-1 ) return *this;
      if (rlen < 0  || rlen>length()-idx) return cut(idx);
	    fCount-=rlen;
	    if (idx<(int)fCount) //copy the zero terminator as well (hence the +2)
		    memmove(&fArray[idx], &fArray[idx+rlen],fCount-idx-rlen+2);
      return *this;
}

inline Gcstr& cut(int idx, int rlen) { return remove(idx, rlen); }

Gcstr substr(int idx = 0, int len = -1) const {
    if (idx < 0)  idx += length();
        else if (idx>=length()) { len=0; idx=length(); }
    if (len) {// A length of -1 specifies the rest of the string.
      if (len < 0  || len>length()-idx) len = length() - idx;
      if (idx<0 || idx>=length() || len<0 ) return Gcstr((int)0);
    }
    if (len) return Gcstr(len, &fArray[idx]);
    return Gcstr((int)0);
}

Gcstr split(const char* delim) {
          //split "this" in two parts, at the first (leftmost)
          //encounter of delim:
          //     1st is left in "this",
          //      2nd part will be returned as a new string!
 int i=index(delim);
 if (i>=0) {
      int i2=i+strlen(delim);
      Gcstr result(length()-i2, &fArray[i2]);
      cut(i);
      return result;
 }
 return Gcstr((int)0);
}

Gcstr split(char c) {
  int i=index(c);
  if (i>=0){
      Gcstr result(length()-i-1, &fArray[i+1]);
      cut(i);
      return result;
      }
  return Gcstr((int)0);
}

 bool contains(const Gcstr& s) const {
    return (index(s.chars()) >= 0);
 }

 bool contains(const char *s) const {
   return (index(s) >= 0);
 }

 bool contains(char c) const {
   return (index(c) >= 0);
 }

Gcstr& appendfmt(const char *fmt, ...) {
// Format as in sprintf
  char* buf; GMALLOC(buf, strlen(fmt)+1024);
  //+1K buffer should be enough for common expressions
  va_list arguments;
  va_start(arguments,fmt);
  vsprintf(buf,fmt,arguments);
  va_end(arguments);
  append(buf);
  GFREE(buf);
  return *this;
  }

};

// - GFStream -- basic file reading stream based on Heng Li's kseq.h

template< typename TFile, typename TFunc > class GFStream {
  public:
    constexpr static byte SEP_SPACE = 0;  // isspace(): \t, \n, \v, \f, \r
    constexpr static byte SEP_TAB = 1;    // isspace() && !' '
    constexpr static byte SEP_LINE = 2;   // line separator: "\n" (Unix) or "\r\n" (Windows)
    constexpr static byte SEP_MAX = 2;
  protected:
     /* Consts */
     constexpr static uint DEFAULT_BUFSIZE = 16384;
     char* buf=NULL;                      // character buffer
     TFile f;
     TFunc rdfunc;
     uint bufsize=0;                   // buffer size
     int begin=0;                     // begin buffer index
     int end=0;                       // end buffer index or error flag if -1
     bool is_eof=false;                   // eof flag

  public:
     GFStream(TFile f_, TFunc rdfunc_, uint bufsize_=DEFAULT_BUFSIZE) : f(f_), rdfunc(rdfunc_), bufsize(bufsize_)
        {
          // f( std::move( f_ ) ), rdfunc( std::move(  rdfunc_  ) )
          GMALLOC(buf, bufsize);
        }
     ~GFStream() { GFREE(buf); }
     bool err() { return ( this->end < 0); }
     bool eof() { return ( this->is_eof  && this->begin >= this->end ); }
     void rewind() { this->is_eof=false; this->begin = this->end = 0; }

     int getc() {
       if (this->err()) return -3;
       if (this->is_eof && this->begin >= this->end) return -1;
		   if (this->begin >= this->end) {
			    this->begin = 0;
			    this->end = this->rdfunc(this->f, this->buf, this->bufsize);
			    if (this->end == 0) { this->is_eof = 1; return -1;}
			    if (this->end == -1) { this->is_eof = 1; return -3;}
       }
		   return (int)buf[this->begin++];
     }

     int getUntil(byte delimiter, Gcstr& str, bool append=false, int* dret=NULL) {
       int gotany = 0;
       if (dret) *dret = 0;
		   //str->l = append? str->l : 0;
       if (!append) str.Reset();
       for (;;) {
         int i;
         if (this->err()) return -3;
         if (this->begin >= this->end) {
           if (!this->is_eof) {
             this->begin = 0;
             this->end = this->rdfunc(this->f, this->buf, this->bufsize);
             if (this->end == 0) { this->is_eof = 1; break; }
             if (this->end == -1) { this->is_eof = 1; return -3; }
           } else break;
         }
         if (delimiter == SEP_LINE) {
           for (i = this->begin; i < this->end; ++i)
             if (this->buf[i] == '\n') break;
         } else if (delimiter > SEP_MAX) {
             for (i = this->begin; i < this->end; ++i)
               if (this->buf[i] == delimiter) break;
         } else if (delimiter == SEP_SPACE) {
            for (i = this->begin; i < this->end; ++i)
              if (isspace(this->buf[i])) break;
         } else if (delimiter == SEP_TAB) {
            for (i = this->begin; i < this->end; ++i)
              //if (isspace(this->buf[i]) && this->buf[i] != ' ') break;
              if (this->buf[i] == '\t') break;
         } else i = 0; // never be here!
          // append from buf from begin to i
         gotany = 1;
         //memcpy(str->s + str->l, this->buf + this->begin, i - this->begin);
         //str->l = str->l + (i - this->begin);
         str.append_buf(this->buf + this->begin, i - this->begin);
         this->begin = i + 1;
         if (i < this->end) {
           if (dret) *dret = this->buf[i];
           break;
         }
       }
       if (!gotany && this->eof()) return -1;
       if (delimiter == SEP_LINE && str.len() > 1 && str.last() == '\r') str.Trim();
       return str.len();
     }


 };


// ************** simple line reading class for text files
//GLineReader -- text line reading/buffering class
class GLineReader {
   bool closeFile;
   //int len;
   //int allocated;
   GDynArray<char> buf;
   int textlen; //length of actual text, without newline character(s)
   bool isEOF;
   FILE* file;
   off_t filepos; //current position
   bool pushed; //pushed back
   int lcount; //line counter (read lines)
 public:
   char* chars() { return buf(); }
   char* line() { return buf(); }
   int readcount() { return lcount; } //number of lines read
   void setFile(FILE* stream) { file=stream; }
   int blength() { return buf.Count(); } //binary/buffer length, including newline character(s)
   int charcount() { return buf.Count(); } //line length, including newline character(s)
   int tlength() { return textlen; } //text length excluding newline character(s)
   int linelen() { return textlen; } //line length, excluding newline character(s)
   //int size() { return buf.Count(); } //same as size();
   bool isEof() {return isEOF; }
   bool eof() { return isEOF; }
   off_t getfpos() { return filepos; }
   off_t getFpos() { return filepos; }
   char* nextLine() { return getLine(); }
   char* getLine() { if (pushed) { pushed=false; return buf(); }
                            else return getLine(file);  }
   char* getLine(FILE* stream) {
                 if (pushed) { pushed=false; return buf(); }
                          else return getLine(stream, filepos); }
   char* getLine(FILE* stream, off_t& f_pos); //read a line from a stream and update
                           // the given file position
   void pushBack() { if (lcount>0) pushed=true; } // "undo" the last getLine request
            // so the next call will in fact return the same line
   GLineReader(const char* fname):closeFile(false),buf(1024), textlen(0),
		   isEOF(false),file(NULL),filepos(0), pushed(false), lcount(0) {
      FILE* f=fopen(fname, "rb");
      if (f==NULL) GError("Error opening file '%s'!\n",fname);
      closeFile=true;
      file=f;
      }
   GLineReader(FILE* stream=NULL, off_t fpos=0):closeFile(false),buf(1024),
		   textlen(0), isEOF(false),file(stream),
		   filepos(fpos), pushed(false), lcount(0) {
     }
   ~GLineReader() {
     if (closeFile) fclose(file);
     }
};


/* extended fgets() -  to read one full line from a file and
  update the file position correctly !
  buf will be reallocated as necessary, to fit the whole line
  */
char* fgetline(char* & buf, int& buflen, FILE* stream, off_t* f_pos=NULL, int* linelen=NULL);


//print int/values nicely formatted in 3-digit groups
char* commaprintnum(int64_t n);

/*********************** File management functions *********************/

// removes the last part (file or directory name) of a full path
// WARNING: this is a destructive operation for the given string!
void delFileName(char* filepath);

// returns a pointer to the last file or directory name in a full path
const char* getFileName(const char* filepath);
// returns a pointer to the file "extension" part in a filename
const char* getFileExt(const char* filepath);


int fileExists(const char* fname);
//returns 0 if path doesn't exist
//        1 if it's a directory
//        2 if it's a regular file
//        3 something else (but entry exists)

int64 fileSize(const char* fpath);

//write a formatted fasta record, fasta formatted
void writeFasta(FILE *fw, const char* seqid, const char* descr,
        const char* seq, int linelen=60, int seqlen=0);

//parses the next number found in a string at the current position
//until a non-digit (and not a '.', 'e','E','-','+') is encountered;
//updates the char* pointer to be after the last digit parsed
bool parseNumber(char* &p, double& v);
bool parseDouble(char* &p, double& v); //just an alias for parseNumber
bool parseFloat(char* &p, float& v);

bool strToInt(char* p, int& i);
bool strToUInt(char* p, uint& i);
bool parseInt(char* &p, int& i); //advance pointer p after the number
bool parseUInt(char* &p, uint& i); //advance pointer p after the number
bool parseHex(char* &p,  uint& i);

#endif // G_BASE_DEFINED
