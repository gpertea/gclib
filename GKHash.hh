/********************************************************************************
 *                  Hash map class templates
 *********************************************************************************/

#ifndef GKHash_HH
#define GKHash_HH
#include "GBase.h"
#include "city.h"
#include "khashl.hpp"
#include <type_traits> //for std::is_trivial

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
	GHashKey(K* k=NULL, bool sh=false):key(k),shared(sh) {}
	~GHashKey() {
		//FIXME: this breaks if K is a class allocated with new!
		if (!shared) {
			//if (std::is_trivial<K>::value)
			  GFREE(key);
			//else delete key;
		}
	}
};

template <class K> struct GHashKey_Eq {
    inline bool operator()(const K* x, const K* y) const {
      return (*x == *y); //requires == operator defined
    }
};

template <class K> struct GHashKey_Hash {
   inline uint32_t operator()(const K* s) const {
      return CityHash32(s, sizeof(s)); //when K is a generic structure
   }
};



//generic set where keys are stored as pointer (to a complex data structure K)
template <class K, class Hash=GHashKey_Hash<K>, class Eq=GHashKey_Eq<K> >
  class GKHashSet:public klib::KHashSetCached< GHashKey<K>*, Hash,  Eq >  {
	//typedef klib::KHashSetCached< GHashKey<K>*, Hash,  Eq > kset_t;
public:
	GKHashSet(); // constructs of an empty hash
	int Add(const K* ky); // return 1 if a key was replaced

	int shkAdd(const K* ky); //return 1 if a key was replaced

	int Remove(const K* ky); //return 0 if no such key was found

	int Find(const K* ky) {//return internal slot location if found,
	                           // or -1 if not found
      uint32_t r=this->get(ky);
      if (r==this->end()) return -1;
      return (int)r;
	}

	inline bool hasKey(const K* ky) { return (this->get(ky)!=this->end()); }

	inline bool operator[](const K* ky) { return (this->get(ky)!=this->end()); }

	void startIterate(); //iterator-like initialization
	const K* Next(); //returns next valid key in the table (NULL if no more)
	void Clear() { clear(); }
	/// Destructor
};

#endif
