#ifndef __AC_KHASHL_HPP
#define __AC_KHASHL_HPP

#include "GBase.h";

/* // ==> Code example <==
#include <cstdio>
#include "khashl.hpp"

int main(void)
{
	klib::KHashMap<uint32_t, int, std::hash<uint32_t> > h; // NB: C++98 doesn't have std::hash
	uint32_t k;
	int absent;
	h[43] = 1, h[53] = 2, h[63] = 3, h[73] = 4;       // one way to insert
	k = h.put(53, &absent), h.value(k) = -2;          // another way to insert
	if (!absent) printf("already in the table\n");    //   which allows to test presence
	if (h.get(33) == h.end()) printf("not found!\n"); // test presence without insertion
	h.del(h.get(43));               // deletion
	for (k = 0; k != h.end(); ++k)  // traversal
		if (h.occupied(k))          // some buckets are not occupied; skip them
			printf("%u => %d\n", h.key(k), h.value(k));
	return 0;
}
*/

uint32_t intHash(uint32_t& pv) {
	uint32_t h=pv;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

uint32_t strHash(char*& ps) {
	char* str=ps;
	int h=0;
	int g;
	while (*str) {
	   h=(h<<4)+*str++;
	    g=h&0xF0000000;
	    if(g) h^=g>>24;
	    //h&=0x0fffffff;
	}
	return h;
}

template<typename T>
bool defaultEqFunc(T& a, T& b) { return a==b; }

template<typename T>
void defaultCopyFunc(T& a, T& b) { a=b; }


namespace klib {

/***********
 * HashSet *
 ***********/

template<class T>
class KHashSet {
	uint32_t bits;
	uint32_t count;
	uint32_t *used;
	T *keys;
	static inline uint32_t __kh_used(const uint32_t *flag, uint32_t i) { return flag[i>>5] >> (i&0x1fU) & 1U; };
	static inline void __kh_set_used(uint32_t *flag, uint32_t i) { flag[i>>5] |= 1U<<(i&0x1fU); };
	static inline void __kh_set_unused(uint32_t *flag, uint32_t i) { flag[i>>5] &= ~(1U<<(i&0x1fU)); };
	static inline uint32_t __kh_fsize(uint32_t m) { return m<32? 1 : m>>5; }
	static inline uint32_t __kh_h2b(uint32_t hash, uint32_t bits) { return hash * 2654435769U >> (32 - bits); }
	static inline uint32_t __kh_h2b(uint64_t hash, uint32_t bits) { return hash * 11400714819323198485ULL >> (64 - bits); }
protected:
	typedef uint32_t _hashFunc(T& a);
	typedef bool _eqFunc(T& a, T& b);
	typedef void _copyFunc(T& a, T& b);
	_hashFunc* hashFunc;
	_eqFunc* eqFunc;
	_copyFunc* copyFunc;
public:
	KHashSet(_hashFunc* hf, _eqFunc* ef = &defaultEqFunc,
			_copyFunc* cf = &defaultCopyFunc) : bits(0), count(0), used(0), keys(0),
			hashFunc(hf), eqFunc(ef), copyFunc(cf) { };
	~KHashSet() { GFREE(used); GFREE(keys); };
	inline uint32_t n_buckets() const { return used? uint32_t(1) << bits : 0; }
	inline uint32_t end() const { return n_buckets(); }
	inline uint32_t size() const { return count; }
	inline T &key(uint32_t x) { return keys[x]; };
	inline bool occupied(uint32_t x) const { return (__kh_used(used, x) != 0); }
	void clear(void) {
		if (!used) return;
		memset(used, 0, __kh_fsize(n_buckets()) * sizeof(uint32_t));
		count = 0;
	}
	uint32_t get(const T &key) const {
		uint32_t i, last, mask, nb;
		if (keys == 0) return 0;
		nb = n_buckets();
		mask = nb - uint32_t(1);
		i = last = __kh_h2b((*hashFunc)(key), bits);
		while (__kh_used(used, i) && !(*eqFunc)(keys[i], key)) {
			i = (i + uint32_t(1)) & mask;
			if (i == last) return nb;
		}
		return !__kh_used(used, i)? nb : i;
	}
	int resize(uint32_t new_nb) {
		uint32_t *new_used = 0;
		uint32_t j = 0, x = new_nb, nb, new_bits, new_mask;
		while ((x >>= uint32_t(1)) != 0) ++j;
		if (new_nb & (new_nb - 1)) ++j;
		new_bits = j > 2? j : 2;
		new_nb = uint32_t(1) << new_bits;
		if (count > (new_nb>>1) + (new_nb>>2)) return 0; // requested size is too small
		new_used = (uint32_t*)std::malloc(__kh_fsize(new_nb) * sizeof(uint32_t));
		memset(new_used, 0, __kh_fsize(new_nb) * sizeof(uint32_t));
		if (!new_used) return -1; // not enough memory
		nb = n_buckets();
		if (nb < new_nb) { // expand
			T *new_keys = (T*)std::realloc(keys, new_nb * sizeof(T));
			if (!new_keys) { GFREE(new_used); return -1; }
			keys = new_keys;
		} // otherwise shrink
		new_mask = new_nb - 1;
		for (j = 0; j != nb; ++j) {
			if (!__kh_used(used, j)) continue;
			T key = keys[j];
			__kh_set_unused(used, j);
			while (1) { // kick-out process; like in Cuckoo hashing
				uint32_t i;
				i = __kh_h2b((*hashFunc)(key), new_bits);
				while (__kh_used(new_used, i)) i = (i + uint32_t(1)) & new_mask;
				__kh_set_used(new_used, i);
				if (i < nb && __kh_used(used, i)) { /* kick out the existing element */
					{ T tmp = keys[i]; keys[i] = key; key = tmp; }
					__kh_set_unused(used, i); /* mark it as deleted in the old hash table */
				} else { // write the element and jump out of the loop
					keys[i] = key;
					break;
				}
			}
		}
		if (nb > new_nb) {/* shrink the hash table */
			//keys = (T*)std::realloc(keys, new_nb * sizeof(T));
			GREALLOC(keys, new_nb * sizeof(T));
		}
		GFREE(used); // free the working space
		used = new_used, bits = new_bits;
		return 0;
	}

	uint32_t put(const T &key, int *absent_ = 0) {
		uint32_t nb, i, last, mask;
		int absent = -1;
		nb = n_buckets();
		if (count >= (nb>>1) + (nb>>2)) { /* rehashing */
			if (resize(nb + uint32_t(1)) < 0) {
				if (absent_) *absent_ = -1;
				return nb;
			}
			nb = n_buckets();
		} // TODO: to implement automatically shrinking; resize() already support shrinking
		mask = nb - 1;
		i = last = __kh_h2b((*hashFunc)(key), bits);
		while (__kh_used(used, i) && !(*eqFunc)(keys[i], key)) {
			i = (i + 1U) & mask;
			if (i == last) break;
		}
		if (!__kh_used(used, i)) { // not present at all
			(*copyFunc)(keys[i], key); //FIXME: for char* this should duplicate the string?
			__kh_set_used(used, i);
			++count, absent = 1;
		} else absent = 0; /* Don't touch keys[i] if present */
		if (absent_) *absent_ = absent;
		return i;
	}
	int del(uint32_t i) {
		uint32_t j = i, k, mask, nb = n_buckets();
		if (keys == 0 || i >= nb) return 0;
		mask = nb - uint32_t(1);
		//FIXME - should keys[i] be deallocated ? (for strings)
		while (1) {
			j = (j + uint32_t(1)) & mask;
			if (j == i || !__kh_used(used, j)) break; // j==i only when the table is completely full
			k = __kh_h2b(hashFunc(&(keys[j])), bits);
			if ((j > i && (k <= i || k > j)) || (j < i && (k <= i && k > j)))
				{ keys[i] = keys[j]; i = j; }
		}
		__kh_set_unused(used, i);
		--count;
		return 1;
	}
};

/***********
 * HashMap *
 ***********/

template<class KType, class VType>
struct KHashMapBucket { KType key; VType val; };

template<class T, class Hash>
struct KHashMapHash { uint32_t operator() (const T &a) const { return Hash()(a.key); } };

//template<class T>
//struct KHashMapEq { bool operator() (const T &a, const T &b) const { return (a.key==b.key); } };

template<class KType, class VType, class Hash>
class KHashMap : public KHashSet< KHashMapBucket<KType, VType> >
		//KHashMapHash<KHashMapBucket<KType, VType>, Hash> >
{
	typedef KHashMapBucket<KType, VType> bucket_t;
	typedef KHashSet<bucket_t> hashset_t; //KHashMapHash<bucket_t, Hash> > hashset_t;
public:
	uint32_t get(const KType &key) const {
		bucket_t t = { key, VType() };
		return hashset_t::get(t);
	}
	uint32_t put(const KType &key, int *absent) {
		bucket_t t = { key, VType() };
		return hashset_t::put(t, absent);
	}
	inline KType &key(khint_t i) { return hashset_t::key(i).key; }
	inline VType &value(khint_t i) { return hashset_t::key(i).val; }
	inline VType &operator[] (const KType &key) {
		bucket_t t = { key, VType() };
		return value(hashset_t::put(t));
	}
};

/****************************
 * HashSet with cached hash *
 ****************************/

template<class KType>
struct KHashSetCachedBucket { KType key; uint32_t hash; };

template<class T>
 struct KHashCachedHash { uint32_t operator() (const T &a) const { return a.hash; } };

//template<class T, class Eq>
//struct KHashCachedEq { bool operator() (const T &a, const T &b) const { return a.hash == b.hash && Eq()(a.key, b.key); } };

template<class KType, class Hash>
class KHashSetCached : public KHashSet< KHashSetCachedBucket<KType> >
  //KHashCachedHash< KHashSetCachedBucket<KType> > >
		// KHashCachedEq<KHashSetCachedBucket<KType, khint_t>, Eq>, khint_t>
{
	typedef KHashSetCachedBucket<KType> bucket_t;
	typedef KHashSet<bucket_t> hashset_t; //, KHashCachedHash<bucket_t>  > hashset_t;
public:
	khint_t get(const KType &key) const {
		bucket_t t = { key, Hash()(key) };
		return hashset_t::get(t);
	}
	khint_t put(const KType &key, int *absent) {
		bucket_t t = { key, Hash()(key) };
		return hashset_t::put(t, absent);
	}
	inline KType &key(uint32_t i) { return hashset_t::key(i).key; }
};

/****************************
 * HashMap with cached hash *
 ****************************/

template<class KType, class VType>
struct KHashMapCachedBucket { KType key; VType val; };

template<class KType, class VType, class Hash >
class KHashMapCached : public KHashSet< KHashMapCachedBucket<KType, VType> >
		// KHashCachedHash<KHashMapCachedBucket<KType, VType> > >
{
	typedef KHashMapCachedBucket<KType, VType> bucket_t;
	typedef KHashSet<bucket_t> hashset_t; //, KHashCachedHash<bucket_t> > hashset_t;
public:
	uint32_t get(const KType &key) const {
		bucket_t t = { key, VType(), Hash()(key) };
		return hashset_t::get(t);
	}
	uint32_t put(const KType &key, int *absent) {
		bucket_t t = { key, VType(), Hash()(key) };
		return hashset_t::put(t, absent);
	}
	inline KType &key(uint32_t i) { return hashset_t::key(i).key; }
	inline VType &value(uint32_t i) { return hashset_t::key(i).val; }
	inline VType &operator[] (const KType &key) {
		bucket_t t = { key, VType(), Hash()(key) };
		return value(hashset_t::put(t));
	}
};

}

#endif /* __AC_KHASHL_HPP */
