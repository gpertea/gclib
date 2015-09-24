/********************************************************************************
*                  Int Hash template (integer keys)                               *
*********************************************************************************/

#ifndef GIHash_HH
#define GIHash_HH
#include "GBase.h"

/**
* This class maintains a fast-access hash table of entities
* indexed by an integer value (i.e. mapping integers to pointers)
*/


template <class OBJ> class GIHash {
 protected:
	struct GIHashRec {
	     int     key;             // Integer key (positive values preferred; -1 means no value)
	     int     hash;        // Hash value of key (-1 means no valid value)
	     pointer data;            // Data
	     };
  GIHashRec* hash;       // array of hash records 
  int         fCapacity;     // table size
  int         fCount;        // number of valid entries
  int  fCurrentEntry;
  GIHashRec* lastptr; //pointer to last GIHashRec added
    //---------- Raw data retrieval (including empty entries
  // Return key at position pos.
  int Key(uint pos) const { return hash[pos].key; }
  // return data OBJ* at given position
  OBJ* Data(uint pos) const { return (OBJ*) hash[pos].data; }
  // Return position of first filled slot, or >= fCapacity
  int First() const;
  // Return position of last filled slot or -1
  int Last() const;
  // Return position of next filled slot in hash table
  // or a value greater than or equal to fCapacity if no filled
  // slot was found
  int Next(int pos) const;
  //Return position of previous filled slot in hash table
  //or a -1 if no filled slot was found
  int Prev(int pos) const;

private:
  GIHash(const GIHash&);
  GIHash &operator=(const GIHash&);
  GFreeProc* fFreeProc; //procedure to free item data
protected:
public:
  static void DefaultFreeProc(pointer item) {
    delete (OBJ*)item;
  }
public:
  GIHash(GFreeProc* freeProc); // constructs of an empty hash
  GIHash(bool doFree=true); // constructs of an empty hash (free the item objects)
  void setFreeItem(GFreeProc *freeProc) { fFreeProc=freeProc; }
  void setFreeItem(bool doFree) { fFreeProc=(doFree)? &DefaultFreeProc : NULL; }
  int Capacity() const { return fCapacity; } // table's size, including the empty slots.
  void Resize(int m);  // Resize the table to the given size.
  int Count() const { return fCount; }// the total number of entries in the table.
  // Insert a new entry into the table for given key
  // If there is already an entry with that key, it will update it!
  const OBJ* Add(const int ky, const OBJ* ptr=NULL);

  // Replace data at key. If there was no existing entry,
  // a new entry is inserted.
  OBJ* Replace(const int ky, const OBJ* ptr);
  // Remove a given key and its data
  OBJ* Remove(const int ky);
  // Find data OBJ* given key.
  OBJ* Find(const int ky);
  bool hasKey(const int ky);
  int getLastKey() { return lastptr ? lastptr->key : -1; }
  OBJ* operator[](const int ky) { return Find(ky); }
  void startIterate(); //iterator-like initialization
  int NextKey(); //returns next valid key in the table (NULL if no more)
  OBJ* NextData(); //returns next valid hash[].data
  OBJ* NextData(int& nextkey); //returns next valid hash[].data
                                //or NULL if no more
                                //nextkey is SET to the corresponding key
  GIHashRec* NextEntry() { //returns a pointer to a GIHashRec
  	 register int pos=fCurrentEntry;
  	 while (pos<fCapacity && hash[pos].hash<0) pos++;
  	 if (pos==fCapacity) {
  	                 fCurrentEntry=fCapacity;
  	                 return NULL;
  	                 }
  	              else {
  	                 fCurrentEntry=pos+1;
  	                 return &hash[pos];
  	                 }
  }
  /// Clear all entries
  void Clear();

  /// Destructor
  virtual ~GIHash();
  };

//------------------------------------------------------------------
// A simpler access to values, as they are stored directly
// in the hash records (instead of pointers to data)
// Requires the OBJ class to have a copy operator (operator=)!
// operator[] behaves differently as r-value vs. l-value:
//  * as r-value returns a pointer (NULL if the key does not exist)
//  * as l-value can be directly assigned an OBJ value (it'll copy it)
//---------------------------------------------------------------------
template <class OBJ> class GIVHash {
 protected:
	struct GIVHashRec {
	     int   key;           // Integer key (positive values preferred; -1 means no value)
	     int   hash;          // Hash value of key (-1 means no valid value)
	     OBJ data;            // Data COPY
	     };
  GIVHashRec* hash;       // array of hash records
  int         fCapacity;     // table size
  int         fCount;        // number of valid entries
  int  fCurrentEntry;
  GIVHashRec* lastptr; //pointer to last GIHashRec accessed (added or retrieved)
    //---------- Raw data retrieval (including empty entries
  // Return key at position pos.
  int Key(uint pos) const { return hash[pos].key; }
  // return data OBJ* at given position
  OBJ* Data(uint pos) const { return (OBJ*) hash[pos].data; }
  // Return position of first filled slot, or >= fCapacity
  int First() const;
  // Return position of last filled slot or -1
  int Last() const;
  // Return position of next filled slot in hash table
  // or a value greater than or equal to fCapacity if no filled
  // slot was found
  int Next(int pos) const;
  //Return position of previous filled slot in hash table
  //or a -1 if no filled slot was found
  int Prev(int pos) const;
	class GIVHashPh {
		//placeholder for a (potential) hash entry
		//needed by operator[]
		friend class GIVHash;
	protected:
		GIVHash<OBJ>& ivhash;
		int key;
	public:
		GIVHashPh(GIVHash<OBJ>& ivh, int k):
			ivhash(ivh), key(k) {}
		GIVHashPh& operator=(const OBJ data) {
		  ivhash.Add(key, data);
		  return *this;
		}
		GIVHashPh& operator=(GIVHashPh& ph) {
			if (this==ph) return *this;
			OBJ* pdata = ph.ivhash.Find(ph.key);
			if (pdata)
			   ivhash.Add(key, *pdata);
			return *this;
		}
		operator OBJ*() {
			return ivhash.Find(key);
		}
	};

private:
  GIVHash(const GIVHash&); //TODO: implement copy constructor
  GIVHash &operator=(const GIVHash&);
public:
  GIVHash(); // constructs of an empty hash
  int Capacity() const { return fCapacity; } // table's size, including the empty slots.
  void Resize(int m);  // Resize the table to the given size.
  int Count() const { return fCount; }// the total number of entries in the table.
  // Insert a new entry into the table for the given key
  // If there is already an entry with that key, it will update it (replace)!
  OBJ* Add(const int ky, const OBJ data);
  OBJ* set(const int ky, const OBJ data) { return Add(ky, data); }
  // Replace data at key. If there was no existing entry,
  // a new entry is inserted.
  OBJ* Replace(const int ky, const OBJ data);
  // Remove a given key and its data
  OBJ Remove(const int ky); //returns a copy of the value removed
  // Find data OBJ* given key.
  OBJ* Find(const int ky) const; //NULL if not present
  bool hasKey(const int ky);
  bool exists(const int ky) { return hasKey(ky); }
  int getLastKey() { return lastptr ? lastptr->key : -1; }
  GIVHashPh operator[](int ky) {
	  return GIVHashPh(*this, ky);
  }
  OBJ* operator[](const int ky) const { return Find(ky);  }

  void startIterate(); //iterator-like initialization
  int NextKey(); //returns next valid key in the table (NULL if no more)
  OBJ* NextData(); //returns next valid hash[].data
  OBJ* NextData(int& nextkey); //returns next valid hash[].data
                                //or NULL if no more
                                //nextkey is SET to the corresponding key
  GIVHashRec* NextEntry() { //returns a pointer to a GIHashRec
  	 register int pos=fCurrentEntry;
  	 while (pos<fCapacity && hash[pos].hash<0) pos++;
  	 if (pos==fCapacity) {
  	                 fCurrentEntry=fCapacity;
  	                 return NULL;
  	                 }
  	              else {
  	                 fCurrentEntry=pos+1;
  	                 return &hash[pos];
  	                 }
  }
  /// Clear all entries
  void Clear();
  /// Destructor
  virtual ~GIVHash();
  };


//
//======================== method definitions ========================
//
/*
  Notes:
  - We store the hash key, so that 99.999% of the time we can compare hash numbers;
    only when hash numbers match do we need to compare keys.
  - The hash table should NEVER get full, or stuff will loop forever!!
*/

// Initial table size (MUST be power of 2)
#define GIHASH_DEF_HASH_SIZE      32
// Maximum hash table load factor (%)
#define GIHASH_MAX_LOAD           80
// Minimum hash table load factor (%)
#define GIHASH_MIN_LOAD           10
// Probe Position [0..n-1]
#define GIHASH1(x,n) (((unsigned int)(x)*13)%(n))
// Probe Distance [1..n-1]
#define GIHASH2(x,n) (1|(((unsigned int)(x)*17)%((n)-1)))

/*******************************************************************************/
// Construct empty hash
template <class OBJ> GIHash<OBJ>::GIHash(GFreeProc* freeProc) {
  GMALLOC(hash, sizeof(GIHashRec)*GIHASH_DEF_HASH_SIZE);
  fCurrentEntry=-1;
  fFreeProc=freeProc;
  lastptr=NULL;
  for (uint i=0; i<GIHASH_DEF_HASH_SIZE; i++)
         hash[i].hash=-1; //this will be an indicator for 'empty' entries
  fCapacity=GIHASH_DEF_HASH_SIZE;
  fCount=0;
  }

template <class OBJ> GIHash<OBJ>::GIHash(bool doFree) {
  GMALLOC(hash, sizeof(GIHashRec)*GIHASH_DEF_HASH_SIZE);
  fCurrentEntry=-1;
  lastptr=NULL;
  fFreeProc = (doFree)?&DefaultFreeProc : NULL;
  for (uint i=0; i<GIHASH_DEF_HASH_SIZE; i++)
         hash[i].hash=-1; //this will be an indicator for 'empty' entries
  fCapacity=GIHASH_DEF_HASH_SIZE;
  fCount=0;
  }


int32 mix_fasthash_64to32(int64 h) { //always returns a positive integer!
        h ^= (h) >> 23;
        h *= 0x2127599bf4325c37ULL;
        h ^= (h) >> 47;
        return (int32)( (h-(h >> 32)) & 0x7FFFFFFF);
}


// Resize table
template <class OBJ> void GIHash<OBJ>::Resize(int m){
  register int i,n,p,h,x;
  GIHashRec *k;
  GASSERT(fCount<=fCapacity);
  if(m<GIHASH_DEF_HASH_SIZE) m=GIHASH_DEF_HASH_SIZE;
  n=fCapacity;
  while((n>>2)>m) n>>=1;            // Shrink until n/4 <= m
  while((n>>1)<m) n<<=1;            // Grow until m <= n/2
  GASSERT(m<=(n>>1));
  GASSERT(GIHASH_DEF_HASH_SIZE<=n);
  if(n!=fCapacity){
    GASSERT(m<=n);
    GMALLOC(k, sizeof(GIHashRec)*n);
    for(i=0; i<n; i++) k[i].hash=-1;
    for(i=0; i<fCapacity; i++){
      h=hash[i].hash;
      if(0<=h){
        p=GIHASH1(h,n);
        GASSERT(0<=p && p<n);
        x=GIHASH2(h,n);
        GASSERT(1<=x && x<n);
        while(k[p].hash!=-1) p=(p+x)%n;
        GASSERT(k[p].hash<0);
        k[p]=hash[i];
        }
      }
    GFREE(hash);
    hash=k;
    fCapacity=n;
    }
  }

// add a new entry, or update it if it already exists
template <class OBJ> const OBJ* GIHash<OBJ>::Add(const int ky,
                      const OBJ* pdata){
  register int p,i,x,h,n;
  //if(!ky) GError("GIHash::insert: NULL key argument.\n");
  GASSERT(fCount<fCapacity);
  //h=strhash(ky);
  h = mix_fasthash_64to32(ky);
  //GASSERT(0<=h);
  p=GIHASH1(h,fCapacity);
  GASSERT(0<=p && p<fCapacity);
  x=GIHASH2(h,fCapacity);
  GASSERT(1<=x && x<fCapacity);
  i=-1;
  n=fCapacity;
  while(n && hash[p].hash!=-1){
    if ((i==-1)&&(hash[p].hash==-2)) i=p;
    if (hash[p].hash==h && hash[p].key==ky) {
      //replace hash data for this key!
      //lastptr=hash[p].key;
      lastptr=& (hash[p]);
      hash[p].data = (pointer) pdata;
      return (OBJ*)hash[p].data;
      }
    p=(p+x)%fCapacity;
    n--;
    }
  if(i==-1) i=p;
  GTRACE(("GIHash::Add: key=\"%s\"\n",ky));
  //GMessage("GIHash::insert: key=\"%s\"\n",ky);
  GASSERT(0<=i && i<fCapacity);
  GASSERT(hash[i].hash<0);
  hash[i].hash=h;
  hash[i].key=ky;
  //hash[i].keyalloc=true;
  //lastptr=hash[i].key;
  lastptr=& hash[i];
  hash[i].data= (pointer) pdata;
  fCount++;
  if((100*fCount)>=(GIHASH_MAX_LOAD*fCapacity)) Resize(fCount);
  GASSERT(fCount<fCapacity);
  return pdata;
  }

// Add or replace entry
template <class OBJ>  OBJ* GIHash<OBJ>::Replace(const int ky,const OBJ* pdata){
  register int p,i,x,h,n;
  //if(!ky){ GError("GIHash::replace: NULL key argument.\n"); }
  GASSERT(fCount<fCapacity);
  //h=strhash(ky);
  h=mix_fasthash_64to32(ky);
  GASSERT(0<=h);
  p=GIHASH1(h,fCapacity);
  GASSERT(0<=p && p<fCapacity);
  x=GIHASH2(h,fCapacity);
  GASSERT(1<=x && x<fCapacity);
  i=-1;
  n=fCapacity;
  while(n && hash[p].hash!=-1){
    if((i==-1)&&(hash[p].hash==-2)) i=p;
    if(hash[p].hash==h && hash[p].key==ky){
      GTRACE(("GIHash::replace: %08x: replacing: \"%s\"\n",this,ky));
      if (fFreeProc!=NULL) (*fFreeProc)(hash[p].data);
      hash[p].data=pdata;
      return hash[p].data;
      }
    p=(p+x)%fCapacity;
    n--;
    }
  if(i==-1) i=p;
  GTRACE(("GIHash::replace: %08x: inserting: \"%s\"\n",this,ky));
  GASSERT(0<=i && i<fCapacity);
  GASSERT(hash[i].hash<0);
  hash[i].hash=h;
  hash[i].key=ky;
  hash[i].data=pdata;
  fCount++;
  if((100*fCount)>=(GIHASH_MAX_LOAD*fCapacity)) Resize(fCount);
  GASSERT(fCount<fCapacity);
  return pdata;
  }


// Remove entry
template <class OBJ> OBJ* GIHash<OBJ>::Remove(const int ky){
  register int p,x,h,n;
  //if(!ky){ GError("GIHash::remove: NULL key argument.\n"); }
  OBJ* removed=NULL;
  if(0<fCount){
    //h=strhash(ky);
    h=mix_fasthash_64to32(ky);
    GASSERT(0<=h);
    p=GIHASH1(h,fCapacity);
    GASSERT(0<=p && p<fCapacity);
    x=GIHASH2(h,fCapacity);
    GASSERT(1<=x && x<fCapacity);
    GASSERT(fCount<fCapacity);
    n=fCapacity;
    while(n && hash[p].hash!=-1){
      if(hash[p].hash==h && hash[p].key==ky){
        //GTRACE(("GIHash::remove: %08x removing: \"%s\"\n",this,ky));
        hash[p].hash=-2;
        //if (hash[p].keyalloc) GFREE((hash[p].key));
        if (fFreeProc!=NULL) (*fFreeProc)(hash[p].data);
            else removed=(OBJ*)hash[p].data;
        hash[p].key=NULL;
        hash[p].data=NULL;
        fCount--;
        if((100*fCount)<=(GIHASH_MIN_LOAD*fCapacity)) Resize(fCount);
        GASSERT(fCount<fCapacity);
        return removed;
        }
      p=(p+x)%fCapacity;
      n--;
      }
    }
  return removed;
  }


// Find entry
template <class OBJ> bool GIHash<OBJ>::hasKey(const int ky) {
  register int p,x,h,n;
  //if(!ky){ GError("GIHash::find: NULL key argument.\n"); }
  if(0<fCount){
    //h=strhash(ky);
    h=mix_fasthash_64to32(ky);
    GASSERT(0<=h);
    p=GIHASH1(h,fCapacity);
    GASSERT(0<=p && p<fCapacity);
    x=GIHASH2(h,fCapacity);
    GASSERT(1<=x && x<fCapacity);
    GASSERT(fCount<fCapacity);
    n=fCapacity;
    while(n && hash[p].hash!=-1){
      if(hash[p].hash==h && hash[p].key==ky){
        return true;
        }
      p=(p+x)%fCapacity;
      n--;
      }
    }
  return false;
}

template <class OBJ> OBJ* GIHash<OBJ>::Find(const int ky){
  register int p,x,n;
  //if(!ky){ GError("GIHash::find: NULL key argument.\n"); }
  if(0<fCount){
    //h=strhash(ky);
    uint32 h=mix_fasthash_64to32(ky);
    GASSERT(0<=h);
    p=GIHASH1(h,fCapacity);
    GASSERT(0<=p && p<fCapacity);
    x=GIHASH2(h,fCapacity);
    GASSERT(1<=x && x<fCapacity);
    GASSERT(fCount<fCapacity);
    n=fCapacity;
    while(n && hash[p].hash!=-1){
      if(hash[p].hash==h && hash[p].key==ky){
        //if (keyptr!=NULL) *keyptr = hash[p].key;
        return (OBJ*)hash[p].data;
        }
      p=(p+x)%fCapacity;
      n--;
      }
    }
  return NULL;
  }

template <class OBJ> void GIHash<OBJ>::startIterate() { //initialize a hash iterator
 fCurrentEntry=0;
}

template <class OBJ> int GIHash<OBJ>::NextKey() {
 register int pos=fCurrentEntry;
 while (pos<fCapacity && hash[pos].hash<0) pos++;
 if (pos==fCapacity) {
                 fCurrentEntry=fCapacity;
                 return NULL;
                 }
              else {
                 fCurrentEntry=pos+1;
                 return hash[pos].key;
                 }
}

template <class OBJ> OBJ* GIHash<OBJ>::NextData() {
 register int pos=fCurrentEntry;
 while (pos<fCapacity && hash[pos].hash<0) pos++;
 if (pos==fCapacity) {
                 fCurrentEntry=fCapacity;
                 return NULL;
                 }
              else {
                 fCurrentEntry=pos+1;
                 return (OBJ*)hash[pos].data;
                 }

}

template <class OBJ> OBJ* GIHash<OBJ>::NextData(int &nextkey) {
 register int pos=fCurrentEntry;
 while (pos<fCapacity && hash[pos].hash<0) pos++;
 if (pos==fCapacity) {
                 fCurrentEntry=fCapacity;
                 nextkey=-1;
                 return NULL;
                 }
              else {
                 fCurrentEntry=pos+1;
                 nextkey=hash[pos].key;
                 return (OBJ*)hash[pos].data;
                 }
}


// Get first non-empty entry
template <class OBJ> int GIHash<OBJ>::First() const {
  register int pos=0;
  while(pos<fCapacity){ if(0<=hash[pos].hash) break; pos++; }
  GASSERT(fCapacity<=pos || 0<=hash[pos].hash);
  return pos;
  }

// Get last non-empty entry
template <class OBJ> int GIHash<OBJ>::Last() const {
  register int pos=fCapacity-1;
  while(0<=pos){ if(0<=hash[pos].hash) break; pos--; }
  GASSERT(pos<0 || 0<=hash[pos].hash);
  return pos;
  }


// Find next valid entry
template <class OBJ> int GIHash<OBJ>::Next(int pos) const {
  GASSERT(0<=pos && pos<fCapacity);
  while(++pos <= fCapacity-1){ if(0<=hash[pos].hash) break; }
  GASSERT(fCapacity<=pos || 0<=hash[pos].hash);
  return pos;
  }


// Find previous valid entry
template <class OBJ> int GIHash<OBJ>::Prev(int pos) const {
  GASSERT(0<=pos && pos<fCapacity);
  while(--pos >= 0){ if(0<=hash[pos].hash) break; }
  GASSERT(pos<0 || 0<=hash[pos].hash);
  return pos;
  }


// Remove all
template <class OBJ> void GIHash<OBJ>::Clear(){
  register int i;
  for(i=0; i<fCapacity; i++){
    if(hash[i].hash>=0){
      //if (hash[i].keyalloc) GFREE((hash[i].key));
      if (fFreeProc!=NULL)
            (*fFreeProc)(hash[i].data);
      }
    }
  GFREE(hash);
  GMALLOC(hash, sizeof(GIHashRec)*GIHASH_DEF_HASH_SIZE);
  //reinitialize it
  for (i=0; i<GIHASH_DEF_HASH_SIZE; i++)
         hash[i].hash=-1; //this will be an indicator for 'empty' entries
  fCapacity=GIHASH_DEF_HASH_SIZE;
  fCount=0;
  }

// Destroy table
template <class OBJ> GIHash<OBJ>::~GIHash(){
  register int i;
  for(i=0; i<fCapacity; i++){
    if(hash[i].hash>=0){
      if (fFreeProc!=NULL) (*fFreeProc)(hash[i].data);
      }
    }
  GFREE(hash);
  }

//-----------------------------------------------------------
//--------------------------- GIHashV -----------------------
template <class OBJ> GIVHash<OBJ>::GIVHash() {
  GMALLOC(hash, sizeof(GIVHashRec)*GIHASH_DEF_HASH_SIZE);
  fCurrentEntry=-1;
  lastptr=NULL;
  for (uint i=0; i<GIHASH_DEF_HASH_SIZE; i++)
         hash[i].hash=-1; //this will be an indicator for 'empty' entries
  fCapacity=GIHASH_DEF_HASH_SIZE;
  fCount=0;
  }

// Resize table
template <class OBJ> void GIVHash<OBJ>::Resize(int m){
  register int i,n,p,h,x;
  GIVHashRec *k;
  GASSERT(fCount<=fCapacity);
  if(m<GIHASH_DEF_HASH_SIZE) m=GIHASH_DEF_HASH_SIZE;
  n=fCapacity;
  while((n>>2)>m) n>>=1;            // Shrink until n/4 <= m
  while((n>>1)<m) n<<=1;            // Grow until m <= n/2
  GASSERT(m<=(n>>1));
  GASSERT(GIHASH_DEF_HASH_SIZE<=n);
  if(n!=fCapacity){
    GASSERT(m<=n);
    GMALLOC(k, sizeof(GIVHashRec)*n);
    for(i=0; i<n; i++) k[i].hash=-1;
    for(i=0; i<fCapacity; i++){
      h=hash[i].hash;
      if(0<=h){
        p=GIHASH1(h,n);
        GASSERT(0<=p && p<n);
        x=GIHASH2(h,n);
        GASSERT(1<=x && x<n);
        while(k[p].hash!=-1) p=(p+x)%n;
        GASSERT(k[p].hash<0);
        k[p]=hash[i];
        }
      }
    GFREE(hash);
    hash=k;
    fCapacity=n;
    }
  }

// add a new entry, or update it if it already exists
template <class OBJ> OBJ* GIVHash<OBJ>::Add(const int ky,
                      const OBJ data){
  register int p,i,x,h,n;
  //if(!ky) GError("GIHashV::insert: NULL key argument.\n");
  GASSERT(fCount<fCapacity);
  //h=strhash(ky);
  h = mix_fasthash_64to32(ky);
  GASSERT(0<=h);

  p=GIHASH1(h,fCapacity);
  GASSERT(0<=p && p<fCapacity);
  x=GIHASH2(h,fCapacity);
  GASSERT(1<=x && x<fCapacity);
  i=-1;
  n=fCapacity;
  while(n && hash[p].hash!=-1){
    if ((i==-1)&&(hash[p].hash==-2)) i=p;
    if (hash[p].hash==h && hash[p].key==ky) {
      //replace hash data for this key!
      //lastptr=hash[p].key;
      lastptr=& (hash[p]);
      hash[p].data = data; //operator=
      return & (hash[p].data);
      }
    p=(p+x)%fCapacity;
    n--;
    }
  if(i==-1) i=p;
  GTRACE(("GIHashV::Add: key=\"%s\"\n",ky));
  //GMessage("GIHashV::insert: key=\"%s\"\n",ky);
  GASSERT(0<=i && i<fCapacity);
  GASSERT(hash[i].hash<0);
  hash[i].hash=h;
  hash[i].key=ky;
  //hash[i].keyalloc=true;
  //lastptr=hash[i].key;
  lastptr=& hash[i];
  hash[i].data = data; //operator=
  fCount++;
  if((100*fCount)>=(GIHASH_MAX_LOAD*fCapacity)) Resize(fCount);
  GASSERT(fCount<fCapacity);
  return &(hash[i].data);
  }

// Add or replace entry
template <class OBJ>  OBJ* GIVHash<OBJ>::Replace(const int ky,const OBJ data){
  register int p,i,x,h,n;
  //if(!ky){ GError("GIHashV::replace: NULL key argument.\n"); }
  GASSERT(fCount<fCapacity);
  //h=strhash(ky);
  h=mix_fasthash_64to32(ky);
  GASSERT(0<=h);
  p=GIHASH1(h,fCapacity);
  GASSERT(0<=p && p<fCapacity);
  x=GIHASH2(h,fCapacity);
  GASSERT(1<=x && x<fCapacity);
  i=-1;
  n=fCapacity;
  while(n && hash[p].hash!=-1){
    if((i==-1)&&(hash[p].hash==-2)) i=p;
    if(hash[p].hash==h && hash[p].key==ky){
      GTRACE(("GIHashV::replace: %08x: replacing: \"%s\"\n",this,ky));
      hash[p].data=data; //copy operator
      return &(hash[p].data);
      }
    p=(p+x)%fCapacity;
    n--;
    }
  if(i==-1) i=p;
  GTRACE(("GIHashV::replace: %08x: inserting: \"%s\"\n",this,ky));
  GASSERT(0<=i && i<fCapacity);
  GASSERT(hash[i].hash<0);
  hash[i].hash=h;
  hash[i].key=ky;
  hash[i].data=data; //copy operator
  fCount++;
  if((100*fCount)>=(GIHASH_MAX_LOAD*fCapacity)) Resize(fCount);
  GASSERT(fCount<fCapacity);
  return &(hash[i].data);
}


// Remove entry
template <class OBJ> OBJ GIVHash<OBJ>::Remove(const int ky){
  register int p,x,h,n;
  //if(!ky){ GError("GIHashV::remove: NULL key argument.\n"); }
  OBJ removed;
  if(0<fCount){
    //h=strhash(ky);
    h=mix_fasthash_64to32(ky);
    GASSERT(0<=h);
    p=GIHASH1(h,fCapacity);
    GASSERT(0<=p && p<fCapacity);
    x=GIHASH2(h,fCapacity);
    GASSERT(1<=x && x<fCapacity);
    GASSERT(fCount<fCapacity);
    n=fCapacity;
    while(n && hash[p].hash!=-1){
      if(hash[p].hash==h && hash[p].key==ky){
        //GTRACE(("GIHashV::remove: %08x removing: \"%s\"\n",this,ky));
        hash[p].hash=-2;
        //if (hash[p].keyalloc) GFREE((hash[p].key));
        removed=hash[p].data; //old data copied
        hash[p].key=-1;
        //hash[p].data=NULL;
        fCount--;
        if((100*fCount)<=(GIHASH_MIN_LOAD*fCapacity)) Resize(fCount);
        GASSERT(fCount<fCapacity);
        return removed;
        }
      p=(p+x)%fCapacity;
      n--;
      }
    }
  return removed;
  }


// Find entry
template <class OBJ> bool GIVHash<OBJ>::hasKey(const int ky) {
  register int p,x,h,n;
  //if(!ky){ GError("GIHashV::find: NULL key argument.\n"); }
  if(0<fCount){
    //h=strhash(ky);
    h=mix_fasthash_64to32(ky);
    GASSERT(0<=h);
    p=GIHASH1(h,fCapacity);
    GASSERT(0<=p && p<fCapacity);
    x=GIHASH2(h,fCapacity);
    GASSERT(1<=x && x<fCapacity);
    GASSERT(fCount<fCapacity);
    n=fCapacity;
    while(n && hash[p].hash!=-1){
      if(hash[p].hash==h && hash[p].key==ky){
        return true;
        }
      p=(p+x)%fCapacity;
      n--;
      }
    }
  return false;
}

template <class OBJ> OBJ* GIVHash<OBJ>::Find(const int ky) const{
  register int p,x,h,n;
  //if(!ky){ GError("GIHashV::find: NULL key argument.\n"); }
  if(0<fCount){
    //h=strhash(ky);
    h=mix_fasthash_64to32(ky);
    //GASSERT(0<=h);
    p=GIHASH1(h,fCapacity);
    GASSERT(0<=p && p<fCapacity);
    x=GIHASH2(h,fCapacity);
    GASSERT(1<=x && x<fCapacity);
    GASSERT(fCount<fCapacity);
    n=fCapacity;
    while(n && hash[p].hash!=-1){
      if(hash[p].hash==h && hash[p].key==ky){
        //if (keyptr!=NULL) *keyptr = hash[p].key;
        return &(hash[p].data);
        }
      p=(p+x)%fCapacity;
      n--;
      }
    }
  return NULL;
  }

template <class OBJ> void GIVHash<OBJ>::startIterate() { //initialize a hash iterator
 fCurrentEntry=0;
}

template <class OBJ> int GIVHash<OBJ>::NextKey() {
 register int pos=fCurrentEntry;
 while (pos<fCapacity && hash[pos].hash<0) pos++;
 if (pos==fCapacity) {
                 fCurrentEntry=fCapacity;
                 return NULL;
                 }
              else {
                 fCurrentEntry=pos+1;
                 return hash[pos].key;
                 }
}

template <class OBJ> OBJ* GIVHash<OBJ>::NextData() {
 register int pos=fCurrentEntry;
 while (pos<fCapacity && hash[pos].hash<0) pos++;
 if (pos==fCapacity) {
                 fCurrentEntry=fCapacity;
                 return NULL;
                 }
              else {
                 fCurrentEntry=pos+1;
                 return (OBJ*)hash[pos].data;
                 }

}

template <class OBJ> OBJ* GIVHash<OBJ>::NextData(int &nextkey) {
 register int pos=fCurrentEntry;
 while (pos<fCapacity && hash[pos].hash<0) pos++;
 if (pos==fCapacity) {
                 fCurrentEntry=fCapacity;
                 nextkey=-1;
                 return NULL;
                 }
              else {
                 fCurrentEntry=pos+1;
                 nextkey=hash[pos].key;
                 return (OBJ*)hash[pos].data;
                 }
}


// Get first non-empty entry
template <class OBJ> int GIVHash<OBJ>::First() const {
  register int pos=0;
  while(pos<fCapacity){ if(0<=hash[pos].hash) break; pos++; }
  GASSERT(fCapacity<=pos || 0<=hash[pos].hash);
  return pos;
  }

// Get last non-empty entry
template <class OBJ> int GIVHash<OBJ>::Last() const {
  register int pos=fCapacity-1;
  while(0<=pos){ if(0<=hash[pos].hash) break; pos--; }
  GASSERT(pos<0 || 0<=hash[pos].hash);
  return pos;
  }


// Find next valid entry
template <class OBJ> int GIVHash<OBJ>::Next(int pos) const {
  GASSERT(0<=pos && pos<fCapacity);
  while(++pos <= fCapacity-1){ if(0<=hash[pos].hash) break; }
  GASSERT(fCapacity<=pos || 0<=hash[pos].hash);
  return pos;
  }


// Find previous valid entry
template <class OBJ> int GIVHash<OBJ>::Prev(int pos) const {
  GASSERT(0<=pos && pos<fCapacity);
  while(--pos >= 0){ if(0<=hash[pos].hash) break; }
  GASSERT(pos<0 || 0<=hash[pos].hash);
  return pos;
  }


// Remove all
template <class OBJ> void GIVHash<OBJ>::Clear(){
  register int i;
  GFREE(hash);
  GMALLOC(hash, sizeof(GIVHashRec)*GIHASH_DEF_HASH_SIZE);
  //reinitialize it
  for (i=0; i<GIHASH_DEF_HASH_SIZE; i++)
         hash[i].hash=-1; //this will be an indicator for 'empty' entries
  fCapacity=GIHASH_DEF_HASH_SIZE;
  fCount=0;
  }

// Destroy table
template <class OBJ> GIVHash<OBJ>::~GIVHash(){
  GFREE(hash);
  }


#endif
