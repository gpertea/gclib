/********************************************************************************
 *                  Hash map class templates
 *********************************************************************************/

#ifndef GKHash_HH
#define GKHash_HH
#include "GBase.h"
#include "city.h"
#include "khashl.hh"
//#include <type_traits> //for std::is_trivial

/*
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
*/

#define GSTR_HASH(s) strhash(s)
//#define GSTR_HASH(s) djb_hash(s)
//#define GSTR_HASH(s) fnv1a_hash(s)
//#define GSTR_HASH(s) murmur3(s)

template <class K> struct GHashKey {
	const K* key; //WARNING: key must be allocated with malloc not new!
	bool shared;
	GHashKey(const K* k=NULL, bool sh=true):key(k),shared(sh) {}
	void own() {
		 shared=false;
		 key=GDupAlloc(*key);
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
	const char* key; //WARNING: key must be allocated with malloc not new!
	bool shared;
	GHashKey(const char* k=NULL, bool sh=true):key(k),shared(sh) {}
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


//generic set where keys are stored as pointers (to a complex data structure K)
//with dedicated specialization for char*
template <class K, class Hash=GHashKey_Hash<K>, class Eq=GHashKey_Eq<K> >
  class GKHashPtrSet:public klib::KHashSetCached< GHashKey<K>, Hash,  Eq >  {
	//typedef klib::KHashSetCached< GHashKey<K>*, Hash,  Eq > kset_t;
protected:
	uint32_t i_iter=0;
public:
	int Add(const K* ky) { // if a key does not exist allocate a copy of the key
                           // return -1 if the key already exists
		GHashKey<K> hk(ky, true);
		int absent=-1;
		uint32_t i=this->put(hk, &absent);
		if (absent==1) { //key was actually added
			this->key(i).own(); //make a copy of the key data
			return i;
		}
		return -1;
	}

	int shkAdd(const K* ky){ //return -1 if the key already exists
		GHashKey<K> hk(ky, true);
		int absent=-1;
		uint32_t i=this->put(hk, &absent);
		if (absent==1) //key was actually added
			return i;
		return -1;
	}

	int Remove(const K* ky) { //return index being removed
		//or -1 if key not found
		  GHashKey<K> hk(ky, true);
		  uint32_t i=this->get(hk);
	      if (i!=this->end()) {
	    	  hk=this->key(i);
	    	  if (this->del(i) && !hk.shared) GFREE(hk.key);
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

	void startIterate() { //iterator-like initialization
	  i_iter=0;
	}

	const K* Next() {
		//returns next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
		return this->key(i_iter).key;
	}

	const K* Next(uint32_t& idx) {
		//returns next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
		idx=i_iter;
		return this->key(i_iter).key;
	}

	inline uint32_t Count() { return this->count; }

	inline void Clear() {
		int nb=this->n_buckets();
		for (int i = 0; i != nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
			GHashKey<K> hk=this->key(i);
			if (!hk.shared) {
				GFREE(hk.key);
			}
		}
		this->clear(); //does not shrink !
	}
	inline void Reset() {
		int nb=this->n_buckets();
		for (int i = 0; i != nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
			GHashKey<K> hk=this->key(i);
			if (!hk.shared) {
				GFREE(hk.key);
			}
		}
		GFREE(this->used); GFREE(this->keys);
		this->bits=0; this->count=0;
	}

    ~GKHashPtrSet() {
    	this->Reset();
    }
};

//generic set where keys are stored as pointers (to a complex data structure K)
//with dedicated specialization for char*
template <class K, class V, class Hash=GHashKey_Hash<K>, class Eq=GHashKey_Eq<K> >
  class GHashMap:public klib::KHashMapCached< GHashKey<K>, V*, Hash,  Eq >  {
	//typedef klib::KHashSetCached< GHashKey<K>*, Hash,  Eq > kset_t;
protected:
	uint32_t i_iter=0;
	bool freeItems;
public:
	GHashMap(bool doFree=true):freeItems(doFree) { };
	int Add(const K* ky, V* val) { // if a key does not exist allocate a copy of the key
                           // return -1 if the key already exists
		GHashKey<K> hk(ky, true);
		int absent=-1;
		uint32_t i=this->put(hk, &absent);
		if (absent==1) { //key was actually added
			this->key(i).own(); //make a copy of the key data
			this->value(i)=val;
			return i;
		}
		return -1;
	}

	int shkAdd(const K* ky, V* val) { //return -1 if the key already exists
		GHashKey<K> hk(ky, true);
		int absent=-1;
		uint32_t i=this->put(hk, &absent);
		if (absent==1) {//key was actually added
			this->value(i)=val; //pointer copy
			return i;
		}
		return -1;
	}

	int Remove(const K* ky) { //return index being removed
		//or -1 if key not found
		  GHashKey<K> hk(ky, true);
		  uint32_t i=this->get(hk);
	      if (i!=this->end()) {
	    	  hk=this->key(i);
	    	  if (freeItems) {
	    		  delete (this->value(i));
	    	  }
	    	  if (this->del(i) && !hk.shared) GFREE(hk.key);
	    	  return i;
	      }
	      return -1;
	}

	V* Find(const K* ky) {//return internal slot location if found,
	                       // or -1 if not found
	  GHashKey<K> hk(ky, true);
      uint32_t r=this->get(hk);
      if (r==this->end()) return NULL;
      return this->value(r);
	}

	inline bool hasKey(const K* ky) {
		GHashKey<K> hk(ky, true);
		return (this->get(hk)!=this->end());
	}


	inline V* & operator[](const K* ky) {
		GHashKey<K> hk(ky, true);
		int absent=-1;
		uint32_t i=this->put(hk, &absent);
		if (absent==1) { //key was not there before
			this->key(i).own(); //make a copy of the key data
			value(i)=NULL;
		}
        return value(i);
	}

	void startIterate() { //iterator-like initialization
	  i_iter=0;
	}

	const K* Next() {
		//returns next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
		return this->key(i_iter).key;
	}

	const K* Next(V*& val) {
		//returns next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) { val=NULL; return NULL;}
		val=this->value(i_iter);
		return this->key(i_iter).key;
	}


	const K* Next(uint32_t& idx) {
		//returns next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
		idx=i_iter;
		return this->key(i_iter).key;
	}

	inline uint32_t Count() { return this->count; }

	inline void Clear() {
		int nb=this->n_buckets();
		for (int i = 0; i != nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
			GHashKey<K> hk=this->key(i);
			if (freeItems) delete this->value(i);
			if (!hk.shared) {
				GFREE(hk.key);
			}

		}
		this->clear(); //does not shrink !
	}
	inline void Reset() {
		int nb=this->n_buckets();
		for (int i = 0; i < nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
			GHashKey<K> hk=this->key(i);
			if (freeItems) delete this->value(i);
			if (!hk.shared) {
				GFREE(hk.key);
			}
		}
		GFREE(this->used); GFREE(this->keys);
		this->bits=0; this->count=0;
	}

    ~GHashMap() {
    	this->Reset();
    }};



#endif
