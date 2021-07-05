#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <type_traits>

template<typename KEY, typename VALUE>
class KHash  {
  uint32_t n_buckets = 0, size_ = 0, n_occupied = 0, upper_bound = 0;
  uint32_t* flags_ = nullptr;
  KEY* keys_ = nullptr;

  static constexpr bool kh_is_map = !std::is_void<VALUE>::value;
  typedef typename std::conditional<kh_is_map, VALUE, char>::type ValueType;
  ValueType* vals_ = nullptr;

 public:
  KHash() { }
  ~KHash() {
    free(flags_);
    free(keys_);
    free(vals_);
  }

  uint32_t get(KEY key) const;
  uint32_t put(KEY key, int* ret);
  void clear();

  // typename std::enable_if<kh_is_map, ValueType>::type&
  ValueType& val(uint32_t x) { return vals_[x]; }

  ValueType get_val(KEY key, ValueType defVal) {
	  uint32_t k = this->get(key);
	  if (k==n_buckets) return defVal;
	  return vals_[k];
  }

  bool set(KEY key, ValueType val) {
	  int ret;
	  uint32_t k=this->put(key, &ret);
	  vals_[k]=val;
	  return (ret);
  }

  void del(uint32_t x);
  uint32_t size() const { return size_; }

 private:
  int resize(uint32_t new_n_buckets);

  //#define __kh_isempty(flag, i) ((flag[i>>4]>>((i&0xfU)<<1))&2)
  //#define __kh_iseither(flag, i) ((flag[i>>4]>>((i&0xfU)<<1))&3)
  //#define __kh_set_isdel_false(flag, i) (flag[i>>4]&=~(1ul<<((i&0xfU)<<1)))
  //#define __kh_set_isempty_false(flag, i) (flag[i>>4]&=~(2ul<<((i&0xfU)<<1)))
  //#define __kh_set_isboth_false(flag, i) (flag[i>>4]&=~(3ul<<((i&0xfU)<<1)))
  //#define __kh_set_isdel_true(flag, i) (flag[i>>4]|=1ul<<((i&0xfU)<<1))

  static bool __kh_isempty(uint32_t* flag, uint32_t i) { return ((flag[i>>4]>>((i&0xfU)<<1))&2); }

  static inline bool __kh_isdel(uint32_t* flag, uint32_t i) { return ((flag[i>>4]>>((i&0xfU)<<1))&1); }

  static inline bool __kh_iseither(uint32_t* flag, uint32_t i) { return ((flag[i>>4]>>((i&0xfU)<<1))&3); }

  static bool __kh_set_isempty_false(uint32_t* flag, uint32_t i)  { return (flag[i>>4]&=~(2ul<<((i&0xfU)<<1))); }

  bool __kh_set_isboth_false(uint32_t i) { return (flags_[i>>4]&=~(3ul<<((i&0xfU)<<1))); }

  bool __kh_set_isdel_true(uint32_t i) { return (flags_[i>>4]|=1ul<<((i&0xfU)<<1)); }

  static uint32_t __hash_func(uint32_t k) { return k; }
  static bool __hash_equal(uint32_t x, uint32_t y) { return x == y; }

  /*
  static uint32_t __hash_func(uint64_t k) { return k; }
  static bool __hash_equal(uint64_t x, uint64_t y) { return x == y; }
  */

  static uint32_t __hash_func(const char *s) {
    // __kh_X31_hash_string
    uint32_t h = (uint32_t)*s;
    if (h) for (++s ; *s; ++s) h = (h << 5) - h + (uint32_t)*s;
    return h;
  }

  static bool __hash_equal(const char* x, const char* y) { return strcmp(x, y) == 0; }

  KHash(const KHash&) = delete;
  void operator=(const KHash&) = delete;

};

#define __kh_kroundup32(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, ++(x))

#define __kh_fsize(m) ((m) < 16? 1 : (m)>>4)

template<typename KEY, typename VALUE>
inline uint32_t KHash<KEY, VALUE>::get(KEY key) const {
	  if (n_buckets) {
	  			uint32_t k, i, last, mask, step = 0;
	  			mask = n_buckets - 1;
	  			k = __hash_func(key);
	  			i = k & mask;
	  			last = i;
	  			while (!__kh_isempty(flags_, i) && (__kh_isdel(flags_,i) ||
	  					!__hash_equal(keys_[i], key))) {
	  				i = (i + (++step)) & mask;
	  				if (i == last) return n_buckets;
	  			}
	  			return __kh_iseither(flags_, i) ? n_buckets : i;
	  		} else return 0;
}

template<typename KEY, typename VALUE>
inline void KHash<KEY, VALUE>::clear() {
	auto h=this;
	if (h->flags_) {											\
		memset(h->flags_, 0xaa, __kh_fsize(h->n_buckets) * sizeof(uint32_t)); \
		h->size_ = h->n_occupied = 0;								\
	}																\
}


template<typename KEY, typename VALUE>
inline uint32_t KHash<KEY, VALUE>::put(KEY key, int* ret) {
  uint32_t x;
  auto h = this;
  if (h->n_occupied >= h->upper_bound) { /* update the hash table */
    if (h->n_buckets > (h->size_<<1)) {
      if (resize(h->n_buckets - 1) < 0) { /* clear "deleted" elements */
        *ret = -1; return h->n_buckets;
      }
    } else if (resize(h->n_buckets + 1) < 0) { /* expand the hash table */
      *ret = -1; return h->n_buckets;
    }
  } /* TODO: to implement automatically shrinking; resize() already support shrinking */
  {
    uint32_t k, i, site, last, mask = h->n_buckets - 1, step = 0;
    x = site = h->n_buckets; k = __hash_func(key); i = k & mask;
    if (__kh_isempty(h->flags_, i)) x = i; /* for speed up */
    else {
      last = i;
      while (!__kh_isempty(h->flags_, i) && (__kh_isdel(h->flags_, i) || !__hash_equal(h->keys_[i], key))) {
        if (__kh_isdel(h->flags_, i)) site = i;
        i = (i + (++step)) & mask;
        if (i == last) { x = site; break; }
      }
      if (x == h->n_buckets) {
        if (__kh_isempty(h->flags_, i) && site != h->n_buckets) x = site;
        else x = i;
      }
    }
  }
  if (__kh_isempty(h->flags_, x)) { /* not present at all */
    h->keys_[x] = key;
    __kh_set_isboth_false(x);
    ++h->size_; ++h->n_occupied;
    *ret = 1;
  } else if (__kh_isdel(h->flags_,x)) { /* deleted */
    h->keys_[x] = key;
    __kh_set_isboth_false(x);
    ++h->size_;
    *ret = 2;
  } else *ret = 0; /* Don't touch h->keys_[x] if present and not deleted */
  return x;
}

template<typename KEY, typename VALUE>
inline int KHash<KEY, VALUE>::resize(uint32_t new_n_buckets) {
/* This function uses 0.25*n_buckets bytes of working space instead of [sizeof(key_t+val_t)+.25]*n_buckets. */

static const double __kh_HASH_UPPER = 0.77;

  auto h = this;
  uint32_t *new_flags = 0;
  uint32_t j = 1;
  {
    __kh_kroundup32(new_n_buckets);
    if (new_n_buckets < 4) new_n_buckets = 4;
    if (h->size_ >= (uint32_t)(new_n_buckets * __kh_HASH_UPPER + 0.5)) j = 0;    /* requested size is too small */
    else { /* hash table size to be changed (shrink or expand); rehash */
      new_flags = (uint32_t*)malloc(__kh_fsize(new_n_buckets) * sizeof(uint32_t));
      if (!new_flags) return -1;
      memset(new_flags, 0xaa, __kh_fsize(new_n_buckets) * sizeof(uint32_t));
      if (h->n_buckets < new_n_buckets) {   /* expand */
        KEY *new_keys = (KEY*)realloc((void *)h->keys_, new_n_buckets * sizeof(KEY));
        if (!new_keys) { free(new_flags); return -1; }
        h->keys_ = new_keys;
        if (kh_is_map) {
          ValueType *new_vals = (ValueType*)realloc((void *)h->vals_, new_n_buckets * sizeof(ValueType));
          if (!new_vals) { free(new_flags); return -1; }
          h->vals_ = new_vals;
        }
      } /* otherwise shrink */
    }
  }
  if (j) { /* rehashing is needed */
    for (j = 0; j != h->n_buckets; ++j) {
      if (__kh_iseither(h->flags_, j) == 0) {
        KEY key = h->keys_[j];
        ValueType val;
        uint32_t new_mask;
        new_mask = new_n_buckets - 1;
        if (kh_is_map) val = h->vals_[j];
        __kh_set_isdel_true(j);
        while (1) { /* kick-out process; sort of like in Cuckoo hashing */
          uint32_t k, i, step = 0;
          k = __hash_func(key);
          i = k & new_mask;
          while (!__kh_isempty(new_flags, i)) i = (i + (++step)) & new_mask;
          __kh_set_isempty_false(new_flags, i);
          if (i < h->n_buckets && __kh_iseither(h->flags_, i) == 0) { /* kick out the existing element */
            { KEY tmp = h->keys_[i]; h->keys_[i] = key; key = tmp; }
            if (kh_is_map) { ValueType tmp = h->vals_[i]; h->vals_[i] = val; val = tmp; }
            __kh_set_isdel_true(i); /* mark it as deleted in the old hash table */
          } else { /* write the element and jump out of the loop */
            h->keys_[i] = key;
            if (kh_is_map) h->vals_[i] = val;
            break;
          }
        }
      }
    }
    if (h->n_buckets > new_n_buckets) { /* shrink the hash table */
      h->keys_ = (KEY*)realloc((void *)h->keys_, new_n_buckets * sizeof(KEY));
      if (kh_is_map) h->vals_ = (ValueType*)realloc((void *)h->vals_, new_n_buckets * sizeof(ValueType));
    }
    free(h->flags_); /* free the working space */
    h->flags_ = new_flags;
    h->n_buckets = new_n_buckets;
    h->n_occupied = h->size_;
    h->upper_bound = (uint32_t)(h->n_buckets * __kh_HASH_UPPER + 0.5);
  }
  return 0;
//#undef kroundup32
//#undef __kh_fsize
}

template<typename KEY, typename VALUE>
inline void KHash<KEY, VALUE>::del(uint32_t x) {
  auto h = this;
  if (x != h->n_buckets && !__kh_iseither(h->flags_, x)) {
    __kh_set_isdel_true(x);
    --h->size_;
  }
}
