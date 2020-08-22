/********************************************************************************
 *                  Hash map class templates
 *********************************************************************************/

#ifndef GKHash_HH
#define GKHash_HH
#include "GBase.h"
#include "city.h"
#include "khashl.hh"
#include <type_traits>
#include <typeinfo>
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

//template<typename T>
// using T_Ptr = typename std::conditional< std::is_pointer<T>::value, T, T* >::type;

/* defined it in GBase.h
template< typename T >
 struct is_char_ptr
  : std::enable_if< std::is_same< T, char* >::value ||
                    std::is_same< T, const char* >::value >
  {};
*/

/*template <typename K, typename=void> struct GCharPtrKey { //K must be a char ptr type
	static_assert(is_char_ptr<K>::value, "GHashKey only takes pointer types");
	const K key; //WARNING: K must be a pointer type and must be malloced
	bool shared;
	GCharPtrKey(const K k=NULL, bool sh=true):key(k),shared(sh) {}
	void own() {
		 shared=false;
		 key=GDupAlloc(*key);
	}
};*/

struct GCharPtrKey {
	const char* key;
	bool shared;
	GCharPtrKey(const char* k=NULL, bool sh=true):key(k),shared(sh) {}
	void own() {
		shared=false;
		char* tmp=Gstrdup(key);
		key=tmp;
	}
};

template<typename T>
 using GHashKeyT = typename std::conditional< is_char_ptr<T>::value,
		 GCharPtrKey, T > ::type ;

template <typename K> struct GHashKey_Eq { //K is a type having the == operator defined
    inline bool operator()(const K& x, const K& y) const {
      return (x == y); //requires == operator to be defined for key type
    }
};

template <> struct GHashKey_Eq<const char*> {
    inline bool operator()(const GCharPtrKey& x, const GCharPtrKey& y) const {
      return (strcmp(x.key, y.key) == 0);
    }
};

/*
template <typename K, typename=void> struct GHashKey_Hash { //K is a pointer type!
   inline uint32_t operator()(const GCharPtrKey& s) const {
	  std::remove_pointer<K> sp;
	  memset((void*)&sp, 0, sizeof(sp)); //make sure the padding is 0
	  sp=*(s.key); //copy key members; key is always a pointer!
      return CityHash32(sp, sizeof(sp)); //when K is a generic structure
   }
};
*/

template <typename K> struct GHashKey_Hash { //K generic (struct or pointer)
   inline uint32_t operator()(const K& s) const {
	  K sp;
	  memset((void*)&sp, 0, sizeof(K)); //make sure the alignment padding for structs is 0
	  sp=s;
      return CityHash32(sp, sizeof(K)); //when K is a generic type
   }
};

template <> struct GHashKey_Hash<const char*> {
   inline uint32_t operator()(const GCharPtrKey& s) const {
      return CityHash32(s.key, strlen(s.key));
   }
};

template <typename K> struct GQKey_Hash { //K generic (struct or pointer)
   inline uint32_t operator()(const K& s) const {
	  //WARNING: alignment padding can mess this up
      return CityHash32((const char *) &s, sizeof(K)); //when K is a generic type
   }
};

template <> struct GQKey_Hash<const char*> {
   inline uint32_t operator()(const char* s) const {
      return CityHash32(s, strlen(s));
   }
};

template <typename K> struct GQKey_Eq { //K is a type having the == operator defined
    inline bool operator()(const K& x, const K& y) const {
      return (x == y); //requires == operator to be defined for key type
    }
};

template <> struct GQKey_Eq<const char*> {
    inline bool operator()(const char* x, const char* y) const {
      return (strcmp(x, y) == 0);
    }
};


//GQSet is never making a deep copy of the char* key, it only stores the pointer
template <typename K=const char*, class Hash=GQKey_Hash<K>, class Eq=GQKey_Eq<K> >
  class GQSet: public std::conditional< is_char_ptr<K>::value,
    klib::KHashSetCached< K, Hash,  Eq >,
	klib::KHashSet< K, Hash,  Eq > >::type  {
public:
	inline int Add(const K ky) { // return -1 if the key already exists
		int absent=-1;
		uint32_t i=this->put(ky, &absent);
		if (absent==1) //key was actually added
		   return i;
		return -1;
	}

	int Remove(K ky) { //return index being removed, or -1 if no such key exists
		  uint32_t i=this->get(ky);
		  if (i!=this->end()) {
			  this->del(i);
	    	  return i;
		  }
		  return -1;
	}

	inline bool operator[](K ky) { //RH only (read-only)
		return (this->get(ky)!=this->end());
	}

	inline void Clear() {
		/*int nb=this->n_buckets();
		for (int i = 0; i != nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
		}*/
		this->clear(); //does not shrink !
	}

	inline void Reset() {
		/*int nb=this->n_buckets();
		for (int i = 0; i != nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
		}*/
		this->clear();
		GFREE(this->used); GFREE(this->keys);
		this->bits=0; this->count=0;
	}
    ~GQSet() {
    	this->Reset();
    }

};

//GQStrSet stores a copy of the added string
template <class Hash=GQKey_Hash<const char*>, class Eq=GQKey_Eq<const char*> >
  class GQStrSet: public klib::KHashSetCached< const char*, Hash,  Eq > {
  public:

	inline int Add(const char* ky) { // return -1 if the key already exists
		int absent=-1;
		uint32_t i=this->put(ky, &absent);
		if (absent==1) {//key was actually added
		   const char* s=Gstrdup(ky);
		   this->key(i)=s; //store a copy of the key string
		   return i;
		}
		//key was already there
		return -1;
	}

	int Remove(const char* ky) { //return index being removed, or -1 if no such key exists
		  uint32_t i=this->get(ky);
		  if (i!=this->end()) {
			  GFREE(this->key(i)); //free string copy
			  this->del(i);
	    	  return i;
		  }
		  return -1;
	}

	inline bool operator[](const char* ky) { //RH only (read-only)
		return (this->get(ky)!=this->end());
	}

	inline void Clear() {
		int nb=this->n_buckets();
		for (int i = 0; i != nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
			//deallocate string copy
			GFREE(this->key(i));
		}
		this->clear(); //does not shrink !
	}

	inline void Reset() {
		this->Clear();
		GFREE(this->used); GFREE(this->keys);
		this->bits=0; this->count=0;
	}
    ~GQStrSet() {
    	this->Reset();
    }

};
/*
template<typename T>
  using GHashKeyT_Eq = typename std::conditional< std::is_pointer<T>::value,
		 GHashKey_Eq<std::remove_pointer<T>>, GHashKey_Eq<T> > ::type;
template<typename T>
  using GHashKeyT_Hash = typename std::conditional< std::is_pointer<T>::value,
		  GHashKey_Hash<std::remove_pointer<T>>, GHashKey_Hash<T> > ::type;
*/
//generic set with dedicated specialization for const char*
//if K is a pointer to any other type, the actual pointer value is used as an integer key (int64_t)
template <typename K=const char*, class Hash=GHashKey_Hash<K>, class Eq=GHashKey_Eq<K> >
  class GHashSet: public std::conditional< is_char_ptr<K>::value,
    klib::KHashSetCached< GHashKeyT<K>, Hash,  Eq >,
	klib::KHashSet< GHashKeyT<K>, Hash,  Eq > >::type  {
	//typedef klib::KHashSetCached< GHashKey<K>*, Hash,  Eq > kset_t;
protected:
	uint32_t i_iter=0;
public:
	template <typename T> typename std::enable_if< is_char_ptr<T>::value, int>::type
	   Add(const T ky) { // if a key does not exist allocate a copy of the key
                          // return -1 if the key already exists
		int absent=-1;
		GCharPtrKey hk(ky);
		uint32_t i=this->put(hk, &absent);
		if (absent==1) { //key was actually added
		   this->key(i).own(); //make a copy of the key data
		   return i;
		}
		return -1;
	}

	template <typename T> typename std::enable_if< !is_char_ptr<T>::value, int>::type
	   Add(const T ky) { // if a key does not exist allocate a copy of the key
                          // return -1 if the key already exists
		int absent=-1;
		uint32_t i=this->put(ky, &absent);
		if (absent==1) //key was actually added
		   return i;
		return -1;
	}

	template <typename T> typename std::enable_if< is_char_ptr<T>::value, int>::type
	   shkAdd(const T ky) { //return -1 if the key already exists
		int absent=-1;
		uint32_t i;
		GCharPtrKey hk(ky);
		i=this->put(hk, &absent);
		if (absent==1) //key was actually added
			return i;
		return -1;
	}
	template <typename T> typename std::enable_if< !is_char_ptr<T>::value, int>::type
	    Remove(T ky) { //return index being removed
		  uint32_t i=this->get(ky);
		  if (i!=this->end()) {
			  this->del(i);
	    	  return i;
		  }
		  return -1;
	}

	template <typename T> typename std::enable_if< is_char_ptr<T>::value, int>::type
	    Remove(T ky) { //return index being removed
		//or -1 if key not found
		  GCharPtrKey hk(ky);
		  uint32_t i=this->get(hk);
		  if (i!=this->end()) {
			  hk=this->key(i);
			  if (this->del(i) && !hk.shared) GFREE(hk.key);
			  return i;
		  }
		  return -1;
	}

	template <typename T> typename std::enable_if< is_char_ptr<T>::value, int>::type
	  Find(T ky) {//return internal slot location if found,
	                       // or -1 if not found
	    GCharPtrKey hk(ky, true);
	    uint32_t r=this->get(hk);
	    if (r==this->end()) return -1;
	    return (int)r;
	}

	template <typename T> typename std::enable_if< !is_char_ptr<T>::value, int>::type
	  Find(T ky) {//return internal slot location if found,
	                       // or -1 if not found
  	  uint32_t r=this->get(ky);
  	  if (r==this->end()) return -1;
  	  return (int)r;
	}

	template <typename T> inline typename std::enable_if< is_char_ptr<T>::value, bool>::type
	 hasKey(T ky) {
		  GCharPtrKey hk(ky, true);
		  return (this->get(hk)!=this->end());
	}

	template <typename T> inline typename std::enable_if< !is_char_ptr<T>::value, bool>::type
	 hasKey(T ky) {
		return (this->get(ky)!=this->end());
	}

	template <typename T> inline typename std::enable_if< is_char_ptr<T>::value, bool>::type
	  operator[](T ky) { //RH only (read-only)
		  GCharPtrKey hk(ky, true);
		  return (this->get(hk)!=this->end());
	}

	template <typename T> inline typename std::enable_if< !is_char_ptr<T>::value, bool>::type
	  operator[](T ky) { //RH only (read-only)
		return (this->get(ky)!=this->end());
	}

	void startIterate() { //iterator-like initialization
	  i_iter=0;
	}

	template <typename T> const typename std::enable_if< is_char_ptr<T>::value, T>::type
	  Next() {
		//returns a pointer to next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
  		return this->key(i_iter).key;
	}

	template <typename T> const typename std::enable_if< !is_char_ptr<T>::value, T* >::type
	  Next() {
		//returns a pointer to next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
		return &(this->key(i_iter));
	}

	inline uint32_t Count() { return this->count; }

	inline void Clear() {
		int nb=this->n_buckets();
		for (int i = 0; i != nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
			if (is_char_ptr<K>::value) {
				GCharPtrKey hk=this->key(i);
				if (!hk.shared) {
					GFREE(hk.key);
				}
			}
		}
		this->clear(); //does not shrink !
	}
	inline void Reset() {
		int nb=this->n_buckets();
		for (int i = 0; i != nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
			if (is_char_ptr<K>::value) {
				GCharPtrKey hk=this->key(i);
				if (!hk.shared) {
					GFREE(hk.key);
				}
			}
		}
		GFREE(this->used); GFREE(this->keys);
		this->bits=0; this->count=0;
	}

    ~GHashSet() {
    	this->Reset();
    }
};

template<typename T>
 using GHashValT = typename std::conditional< std::is_pointer<T>::value,
		 T, T* > ::type ;


//generic hash map where keys and values can be of any type
template <class K, class V, class Hash=GHashKey_Hash<K>, class Eq=GHashKey_Eq<K> >
  class GHashMap:public std::conditional< is_char_ptr<K>::value,
    klib::KHashMapCached< GHashKeyT<K>, V, Hash,  Eq >,
    klib::KHashMap< GHashKeyT<K>, V, Hash,  Eq > >::type  {

protected:
	uint32_t i_iter=0;
	bool freeItems;
public:
	GHashMap(bool doFree=true):freeItems(doFree) {
		if (! std::is_pointer<V>::value) freeItems=false;
	};
	template <typename T> typename std::enable_if< is_char_ptr<T>::value, int>::type
	  Add(const T ky, V val) { // if a key does not exist allocate a copy of the key
		// return -1 if the key already exists
		int absent=-1;
		GCharPtrKey hk(ky);
		uint32_t i=this->put(hk, &absent);
		if (absent==1) { //key was actually added
		   this->key(i).own(); //make a copy of the key data
			this->value(i)=val;
			return i;
		}
		return -1;
	}
	template <typename T> typename std::enable_if< !is_char_ptr<T>::value, int>::type
	  Add(const T ky, V val) { // if a key does not exist allocate a copy of the key
		// return -1 if the key already exists
		int absent=-1;
		uint32_t i=this->put(ky, &absent);
		if (absent==1) { //key was actually added
			this->value(i)=val;
			return i;
		}
		return -1;
	}



	template <typename T> typename std::enable_if< is_char_ptr<T>::value, int>::type
	   shkAdd(const T ky, V val) { //return -1 if the key already exists
		GCharPtrKey hk(ky, true);
		int absent=-1;
		uint32_t i=this->put(hk, &absent);
		if (absent==1) {//key was actually added
			this->value(i)=val;
			return i;
		}
		return -1;
	}
	template <typename T> typename std::enable_if< !is_char_ptr<T>::value, int>::type
	    Remove(T ky) { //return index being removed
		  uint32_t i=this->get(ky);
		  if (i!=this->end()) {
			  this->del(i);
	    	  if (freeItems) {
	    		  delete (this->value(i));
	    	  }
	    	  return i;
		  }
		  return -1;
	}

	template <typename T> typename std::enable_if< is_char_ptr<T>::value, int>::type
	    Remove(T ky) { //return index being removed
		//or -1 if key not found
		  GCharPtrKey hk(ky);
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

	template <typename T> typename std::enable_if< is_char_ptr<T>::value, int>::type
	  findIdx(T ky) {//return internal slot location if found,
	                       // or -1 if not found
	    GCharPtrKey hk(ky, true);
	    uint32_t r=this->get(hk);
	    if (r==this->end()) return -1;
	    //return &(this->value(r));
	    return (int)r;
	}

	template <typename T> typename std::enable_if< !is_char_ptr<T>::value, int>::type
	  findIdx(T ky) {//return internal slot location if found,
	                       // or -1 if not found
  	  uint32_t r=this->get(ky);
  	  if (r==this->end()) return -1;
  	  //return &(this->value(r));
  	  return (int)r;
	}

	//return pointer to stored value if found, NULL otherwise
	// if the stored value is a pointer, it's going to be a pointer to that
	V* Find(const K ky) {
	  int r=this->findIdx(ky);
	  if (r<0) return NULL;
	  return &(this->value(r));
	}

	template <typename T> inline typename std::enable_if< is_char_ptr<T>::value, bool>::type
	 hasKey(T ky) {
		  GCharPtrKey hk(ky, true);
		  return (this->get(hk)!=this->end());
	}

	template <typename T> inline typename std::enable_if< !is_char_ptr<T>::value, bool>::type
	 hasKey(T ky) {
		return (this->get(ky)!=this->end());
	}

/*
	inline V& operator[](const K* ky) {
		GHashKey<K> hk(ky, true);
		int absent=-1;
		uint32_t i=this->put(hk, &absent);
		if (absent==1) { //key was not there before
			this->key(i).own(); //make a copy of the key data
			//value(i)=NULL;
		}
        return value(i);
	}
*/
	void startIterate() { //iterator-like initialization
	  i_iter=0;
	}

	template <typename T> const typename std::enable_if< is_char_ptr<T>::value, T>::type
	  Next(V*& val) {
		//returns a pointer to next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
  		return this->key(i_iter).key;
	}

	template <typename T> const typename std::enable_if< !is_char_ptr<T>::value, T* >::type
	  Next(V*& val) {
		//returns a pointer to next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) { val=NULL; return NULL; }
		val=&(this->value(i_iter));
		return &(this->key(i_iter));
	}

	template <typename T> const typename std::enable_if< is_char_ptr<T>::value, T>::type
	  Next(uint32_t& idx) {
		//returns a pointer to next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
		idx=i_iter;
  		return this->key(i_iter).key;
	}

	template <typename T> const typename std::enable_if< !is_char_ptr<T>::value, T* >::type
	  Next(uint32_t& idx) {
		//returns a pointer to next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
		idx=i_iter;
		return &(this->key(i_iter));
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
			if (is_char_ptr<K>::value) {
				GCharPtrKey hk=this->key(i);
				if (!hk.shared) {
					GFREE(hk.key);
				}
			}
			if (freeItems) delete this->value(i);
		}
		this->clear(); //does not shrink !
	}

	inline void Reset() {
		int nb=this->n_buckets();
		for (int i = 0; i < nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
			if (is_char_ptr<K>::value) {
				GCharPtrKey hk=this->key(i);
				if (!hk.shared) {
					GFREE(hk.key);
				}
			}
			if (freeItems) delete this->value(i);
		}
		GFREE(this->used); GFREE(this->keys);
		this->bits=0; this->count=0;
	}

    ~GHashMap() {
    	this->Reset();
    }};



#endif
