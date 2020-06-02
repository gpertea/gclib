/********************************************************************************
 *                  Hash map class templates
 *********************************************************************************/

#ifndef GKHash_HH
#define GKHash_HH
#include "GBase.h"
#include "city.h"
#include "khashl.hpp"
//#include <type_traits> //for std::is_trivial

struct cstr_eq {
    inline bool operator()(const char* x, const char* y) const {
      return (strcmp(x, y) == 0);
    }
};

struct cstr_hash {
   inline uint32_t operator()(const char* s) const {
      return CityHash32(s, std::strlen(s));
   }
};


#define GSTR_HASH(s) strhash(s)
//#define GSTR_HASH(s) djb_hash(s)
//#define GSTR_HASH(s) fnv1a_hash(s)
//#define GSTR_HASH(s) murmur3(s)

template <class K> struct GHashKey {
	K* key; //WARNING: key must be allocated with malloc not new!
	bool shared;
	GHashKey(K* k=NULL, bool sh=true):key(k),shared(sh) {}
	void own() {
		 K* tmp=NULL;
		 shared=false;
		 GALLOCDUP(tmp, key);
		 key=tmp;
	}
	/*
	~GHashKey() {
		//FIXME: this breaks if K is a class allocated with new!
		if (!shared) {
			//if (std::is_trivial<K>::value)
			  GFREE(key);
			//else delete key;
		}
	}
	*/
};

template <> struct GHashKey<char> {
	char* key; //WARNING: key must be allocated with malloc not new!
	bool shared;
	GHashKey(char* k=NULL, bool sh=true):key(k),shared(sh) {}
	void own() {
		shared=false;
		char*tmp=Gstrdup(key);
		key=tmp;
	}

};



template <class K> struct GHashKey_Eq {
    inline bool operator()(const GHashKey<K>& x, const GHashKey<K>& y) const {
      return (*x.key == *y.key); //requires == operator to be defined
    }
};

template <class K> struct GHashKey_Hash {
   inline uint32_t operator()(const GHashKey<K>& s) const {
	  K sp;
	  memset((void*)&sp, 0, sizeof(sp)); //make sure the padding is 0
	  sp=*s.key;
      return CityHash32(sp, sizeof(K)); //when K is a generic structure
   }
};

template <> struct GHashKey_Eq<char> {
    inline bool operator()(const GHashKey<char>& x, const GHashKey<char>& y) const {
      return (strcmp(x.key, y.key) == 0);
    }
};

template <> struct GHashKey_Hash<char> {
   inline uint32_t operator()(const GHashKey<char>& s) const {
      return CityHash32(s.key, strlen(s.key)); //when K is a generic structure
   }
};


//generic set where keys are stored as pointer (to a complex data structure K)
template <class K, class Hash=GHashKey_Hash<K>, class Eq=GHashKey_Eq<K> >
  class GKHashSet:public klib::KHashSetCached< GHashKey<K>, Hash,  Eq >  {
	//typedef klib::KHashSetCached< GHashKey<K>*, Hash,  Eq > kset_t;
public:
	int Add(const K* ky) { // if a key does not exist allocate a copy of the key
                           // return -1 if the key already exists
		GHashKey<K> hk(ky, true);
		int absent=-1;
		khint_t i=put(hk, &absent);
		if (absent==1) { //key was actually added
			key(i).own(); //make a copy of the key data
			return i;
		}
		return -1;
	}

	int shkAdd(const K* ky){ //return -1 if the key already exists
		GHashKey<K> hk(ky, true);
		int absent=-1;
		khint_t i=put(hk, &absent);
		if (absent==1) //key was actually added
			return 1;
		return -1;
	}

	int Remove(const K* ky) { //return index being removed
		//or -1 if key not found
		  GHashKey<K> hk(ky, true);
	      uint32_t i=this->get(hk);
	      if (i!=this->end() && i<n_buckets()) {
	    	  hk=key(i);
	    	  if (del(i) && !hk.shared) GFREE(hk.key);
	    	  return i;
	      }
	      return -1;
	}

	int Find(const K* ky) {//return internal slot location if found,
	                       // or -1 if not found
	  GHashKey<K> hk(ky, true);
      uint32_t r=this->get(hk);
      if (r==this->end()) return -1;
      return (int)r;
	}

	inline bool hasKey(const K* ky) {
		GHashKey<K> hk(ky, true);
		return (this->get(hk)!=this->end());
	}

	inline bool operator[](const K* ky) {
		GHashKey<K> hk(ky, true);
		return (this->get(hk)!=this->end());
	}

	void startIterate(); //iterator-like initialization
	const K* Next(); //returns next valid key in the table (NULL if no more)
	void Clear() { clear(); }
	/// Destructor
};

#endif
