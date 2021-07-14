#ifndef _KRadixSorter_HH
#define _KRadixSorter_HH

#include <cstdint>
#include <cassert>

template <typename REC, typename UINT, UINT REC::* KeyPtr> class KRadixSorter {
   protected:
    static const int RS_MIN_SIZE = 64;
    static const int RS_MAX_BITS = 8;
    struct RdxSortBucket { REC *b, *e; };
    static void insert_sort(REC *beg, REC *end) {
      	REC *i;
       	for (i = beg + 1; i < end; ++i)
       		if ((*i).*KeyPtr < (*(i - 1)).*KeyPtr) {
       			REC *j, tmp = *i;
       			for (j = i; j > beg && tmp.*KeyPtr < (*(j-1)).*KeyPtr; --j)
       				*j = *(j - 1);
       			*j = tmp;
       		}
       }

    static void radix_sort(REC *beg, REC *end, int n_bits, int s) {
    	REC *i;
    	UINT size = 1<<n_bits, m = size - 1;
    	RdxSortBucket *k, b[1<<RS_MAX_BITS], *be = b + size;
    	assert(n_bits <= RS_MAX_BITS);
    	for (k = b; k != be; ++k) k->b = k->e = beg;
    	for (i = beg; i != end; ++i) ++b[(*i).*KeyPtr>>s&m].e;
    	for (k = b + 1; k != be; ++k)
    		k->e += (k-1)->e - beg, k->b = (k-1)->e;
    	for (k = b; k != be;) {
    		if (k->b != k->e) {
    			RdxSortBucket *l;
    			if ((l = b + ((*k->b).*KeyPtr>>s&m)) != k) {
    				AIRegData tmp = *k->b, swap;
    				do {
    					swap = tmp; tmp = *l->b; *l->b++ = swap;
    					l = b + (tmp.*KeyPtr>>s&m);
    				} while (l != k);
    				*k->b++ = tmp;
    			} else ++k->b;
    		} else ++k;
    	}
    	for (b->b = beg, k = b + 1; k != be; ++k) k->b = (k-1)->e;
    	if (s) {
    		s = s > n_bits? s - n_bits : 0;
    		for (k = b; k != be; ++k)
    			if (k->e - k->b > RS_MIN_SIZE) radix_sort(k->b, k->e, n_bits, s);
    			else if (k->e - k->b > 1) insert_sort(k->b, k->e);
    	}
    }

   public:
    void sort(REC *beg, size_t nrecs) {
    	REC *end = beg+nrecs;
    	if (nrecs <= RS_MIN_SIZE) insert_sort(beg, end);
    	else radix_sort(beg, end, RS_MAX_BITS, (sizeof(UINT) - 1) * RS_MAX_BITS);
    }
};

#endif
