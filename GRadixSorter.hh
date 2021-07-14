#ifndef _GRadixSorter_HH
#define _GRadixSorter_HH
#include <algorithm>
#include <memory>
#include <array>
#include <cassert>

template <typename REC, typename UINT, UINT REC::* KeyPtr> class GRadixSorter {
   protected:
     static const int  RADIX_BITS   = 8;
     static const int    RADIX_SIZE   = (UINT)1 << RADIX_BITS;
     static const int    RADIX_LEVELS = (((sizeof(UINT)*8)-1) / RADIX_BITS) + 1;
     static const UINT  RADIX_MASK   = RADIX_SIZE - 1;
     using freq_array_type = UINT [RADIX_LEVELS][RADIX_SIZE];


     std::unique_ptr< REC[] > qreg;
     size_t qreg_len = 0;
     static void count_frequency(REC *a, UINT count, freq_array_type freqs) {
       for (UINT i = 0; i < count; i++) {
            UINT value = a[i].*KeyPtr;
            for (UINT pass = 0; pass < RADIX_LEVELS; pass++) {
                freqs[pass][value & RADIX_MASK]++;
                value >>= RADIX_BITS;
            }
        }
    }
    /* Determine if the frequencies for a given level are "trivial".
    *
    * Frequencies are trivial if only a single frequency has non-zero
    * occurrences. In that case, the radix step just acts as a copy so we can
    * skip it.  */
    static bool is_trivial(UINT freqs[RADIX_SIZE], UINT count) {
        for (UINT i = 0; i < RADIX_SIZE; i++) {
            auto freq = freqs[i];
            if (freq != 0) {
                return freq == count;
            }
        }
        assert(count == 0); // we only get here if count was zero
        return true;
    }
  public:
    void sort(REC *a, size_t count) {
        freq_array_type freqs = {};
        if (qreg_len< count) {
        	qreg = std::unique_ptr<REC[]>(new REC[count]);
        	qreg_len=count;
        }
        count_frequency(a, count, freqs);

        REC *from = a, *to = qreg.get();

        for (UINT pass = 0; pass < RADIX_LEVELS; pass++) {

            if (is_trivial(freqs[pass], count)) {
                // this pass would do nothing, just skip it
                continue;
            }

            UINT shift = pass * RADIX_BITS;

            // array of pointers to the current position in each queue, which we set up based on the
            // known final sizes of each queue (i.e., "tighly packed")
            REC * queue_ptrs[RADIX_SIZE], * next = to;
            for (UINT i = 0; i < RADIX_SIZE; i++) {
                queue_ptrs[i] = next;
                next += freqs[pass][i];
            }

            // copy each element into the appropriate queue based on the current RADIX_BITS sized
            // "digit" within it
            for (UINT i = 0; i < count; i++) {
                //UINT value = RegDataKEY(from[i]);
                UINT value = from[i].*KeyPtr;
                UINT index = (value >> shift) & RADIX_MASK;
                *queue_ptrs[index]++ = from[i]; //value
            }

            // swap from and to areas
            std::swap(from, to);
        }

        // because of the last swap, the "from" area has the sorted payload: if it's
        // not the original array "a", do a final copy
        if (from != a) {
            std::copy(from, from + count, a);
        }
    }
};

#endif
