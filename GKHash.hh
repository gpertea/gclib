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

template<typename T>
 using T_Ptr = typename std::conditional< std::is_pointer<T>::value, T, T* >::type;

template< typename T >
 struct is_char_ptr
  : std::enable_if< std::is_same< T, char* >::value ||
                    std::is_same< T, const char* >::value >
  {};


template <typename K, typename=void> struct GHashKey { //K must be a pointer type!
	static_assert(std::is_pointer<K>::value, "GHashKey only takes pointer types");
	const K key; //WARNING: K must be a pointer type and must be malloced
	bool shared;
	GHashKey(const K k=NULL, bool sh=true):key(k),shared(sh) {}
	void own() {
		 shared=false;
		 key=GDupAlloc(*key);
	}
};

template <typename K>  struct GHashKey<K, typename std::enable_if<
   is_char_ptr<K>::value >::type > {
	const char* key;
	bool shared;
	GHashKey(const char* k=NULL, bool sh=true):key(k),shared(sh) {}
	void own() {
		shared=false;
		char* tmp=Gstrdup(key);
		key=tmp;
	}

};

template<typename T>
 using GHashKeyT = typename std::conditional< std::is_pointer<T>::value,
		 GHashKey<T>, T > ::type ;

template <typename K, typename=void> struct GHashKey_Eq { //K is a pointer type
    inline bool operator()(const GHashKey<K>& x, const GHashKey<K>& y) const {
      return (*x.key == *y.key); //requires == operator to be defined for key type
    }
};

template <typename K> struct GHashKey_Eq<K, typename std::enable_if<
		   is_char_ptr<K>::value >::type> {
    inline bool operator()(const GHashKey<const char*>& x, const GHashKey<const char*>& y) const {
      return (strcmp(x.key, y.key) == 0);
    }
};

template <typename K> struct GHashKey_Eq<K, typename std::enable_if<
  !std::is_pointer<K>::value >::type> { //K is not a pointer type
    inline bool operator()(const K& x, const K& y) const {
      return (x == y); //requires == operator to be defined for key type
    }
};

template <class K, typename=void> struct GHashKey_Hash { //K is a pointer type!
   inline uint32_t operator()(const GHashKey<K>& s) const {
	  std::remove_pointer<K> sp;
	  memset((void*)&sp, 0, sizeof(sp)); //make sure the padding is 0
	  sp=*(s.key); //copy key members; key is always a pointer!
      return CityHash32(sp, sizeof(sp)); //when K is a generic structure
   }
};

template <class K> struct GHashKey_Hash<K, typename std::enable_if<
!std::is_pointer<K>::value >::type> { //K not a pointer type
   inline uint32_t operator()(const K& s) const {
	  K sp;
	  memset((void*)&sp, 0, sizeof(K)); //make sure the padding is 0
	  sp=s;
      return CityHash32(sp, sizeof(K)); //when K is a generic non pointer
   }
};

template <typename K> struct GHashKey_Hash<K, typename std::enable_if<
 is_char_ptr<K*>::value >::type> {
   inline uint32_t operator()(const GHashKey<const char*>& s) const {
      return CityHash32(s.key, strlen(s.key)); //when K is a generic structure
   }
};

template<typename T>
  using GHashKeyT_Eq = typename std::conditional< std::is_pointer<T>::value,
		 GHashKey_Eq<std::remove_pointer<T>>, GHashKey_Eq<T> > ::type;
template<typename T>
  using GHashKeyT_Hash = typename std::conditional< std::is_pointer<T>::value,
		  GHashKey_Hash<std::remove_pointer<T>>, GHashKey_Hash<T> > ::type;

//generic set where keys are stored as pointers (to a complex data structure K)
//with dedicated specialization for char*
template <class K, class Hash=GHashKeyT_Hash<K>, class Eq=GHashKeyT_Eq<K> >
  class GKHashSet:public std::conditional< std::is_pointer<K>::value,
    klib::KHashSetCached< GHashKeyT<K>, Hash,  Eq >, klib::KHashSet< GHashKeyT<K>, Hash,  Eq > >::type  {
	//typedef klib::KHashSetCached< GHashKey<K>*, Hash,  Eq > kset_t;
protected:
	uint32_t i_iter=0;
public:
	int Add(const K ky) { // if a key does not exist allocate a copy of the key
                          // return -1 if the key already exists
		int absent=-1;
		if (std::is_pointer<K>::value) {
			GHashKey<K> hk(ky);
			uint32_t i=this->put(hk, &absent);
			if (absent==1) { //key was actually added
			   this->key(i).own(); //make a copy of the key data
			   return i;
			}
			return -1;
		}
		uint32_t i=this->put(ky, &absent);
		if (absent==1) //key was actually added
		   return i;
		return -1;
	}

	int shkAdd(const K ky){ //return -1 if the key already exists
		int absent=-1;
		uint32_t i;
		if (std::is_pointer<K>::value) {
		  GHashKey<K> hk(ky);
		  i=this->put(hk, &absent);
		} else {
		  i=this->put(ky, &absent);
		}
		if (absent==1) //key was actually added
			return i;
		return -1;
	}

	int Remove(const K ky) { //return index being removed
		//or -1 if key not found
		  GHashKey<K> hk(ky);
		  uint32_t i;
		  if (std::is_pointer<K>::value) {
			  i=this->get(hk);
		      if (i!=this->end()) {
		    	  hk=this->key(i);
		    	  if (this->del(i) && !hk.shared) GFREE(hk.key);
		    	  return i;
		      }
		      return -1;
		  }
		  i=this->get(ky);
	      if (i!=this->end())
	    	  return i;
	      return -1;
	}

	int Find(const K ky) {//return internal slot location if found,
	                       // or -1 if not found
  	 uint32_t r;
     if (std::is_pointer<K>::value) {
	    GHashKey<K> hk(ky, true);
        r=this->get(hk);
     } else {
    	 r=this->get(ky);
     }
      if (r==this->end()) return -1;
      return (int)r;
	}

	inline bool hasKey(const K ky) {
		if (std::is_pointer<K>::value) {
		  GHashKey<K> hk(ky, true);
		  return (this->get(hk)!=this->end());
		}
		return (this->get(ky)!=this->end());
	}

	inline bool operator[](const K ky) {
		if (std::is_pointer<K>::value) {
		  GHashKey<K> hk(ky, true);
		  return (this->get(hk)!=this->end());
		}
		return (this->get(ky)!=this->end());
	}

	void startIterate() { //iterator-like initialization
	  i_iter=0;
	}

	const K* Next() {
		//returns a pointer to next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
		if (std::is_pointer<K>::value)
  		  return this->key(i_iter).key;
		return &(this->key(i_iter));
	}

	const K* Next(uint32_t& idx) {
		//returns next valid key in the table (NULL if no more)
		if (this->count==0) return NULL;
		int nb=this->n_buckets();
		while (i_iter<nb && !this->occupied(i_iter)) i_iter++;
		if (i_iter==nb) return NULL;
		idx=i_iter;
		if (std::is_pointer<K>::value)
  		  return this->key(i_iter).key;
		return &(this->key(i_iter));
	}

	inline uint32_t Count() { return this->count; }

	inline void Clear() {
		int nb=this->n_buckets();
		for (int i = 0; i != nb; ++i) {
			if (!this->__kh_used(this->used, i)) continue;
			if (std::is_pointer<K>::value) {
				GHashKey<K> hk=this->key(i);
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
			if (std::is_pointer<K>::value) {
				GHashKey<K> hk=this->key(i);
				if (!hk.shared) {
					GFREE(hk.key);
				}
			}
		}
		GFREE(this->used); GFREE(this->keys);
		this->bits=0; this->count=0;
	}

    ~GKHashSet() {
    	this->Reset();
    }
};

//generic set where keys are stored as pointers (to a complex data structure K)
//with dedicated specialization for char*
template <class K, class V, class Hash=GHashKey_Hash<K>, class Eq=GHashKey_Eq<K> >
  class GHashMap:public klib::KHashMapCached< GHashKey<K>, V*, Hash,  Eq >  {


protected:
	uint32_t i_iter=0;
	bool freeItems;
	template<typename> struct v_ret;
	template<V> struct v_ret<V> {
		if (std::is_pointer<V>::value) {
			using type = V;
		}
		else { using type = V*; }
	}
public:
	GHashMap(bool doFree=true):freeItems(doFree) {
		if (! std::is_pointer<V>::value) freeItems=false;
	};
	int Add(const K* ky, V val) { // if a key does not exist allocate a copy of the key
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



	int shkAdd(const K* ky, V val) { //return -1 if the key already exists
		GHashKey<K> hk(ky, true);
		int absent=-1;
		uint32_t i=this->put(hk, &absent);
		if (absent==1) {//key was actually added
			this->value(i)=val; //pointer copy if V is a pointer
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

	void* Find(const K* ky) {//return pointer to internal slot index if found
	  GHashKey<K> hk(ky, true);
      uint32_t r=this->get(hk);
      if (r==this->end()) return NULL; //typecast needed here for non-primitive types
      //if (isPointer<V>::value)
      //   return this->value(r);
      return &(this->value(r));
	}

	inline bool hasKey(const K* ky) {
		GHashKey<K> hk(ky, true);
		return (this->get(hk)!=this->end());
	}


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

	const inline V& operator[](const K* ky) {

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
