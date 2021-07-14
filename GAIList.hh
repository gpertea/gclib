//=====================================================================================
//Common structs, parameters, functions
//by Jianglin Feng  09/5/2018
//-------------------------------------------------------------------------------------

#ifndef __GAILIST_H__
#define __GAILIST_H__
//-------------------------------------------------------------------------------------
#include "GHashMap.hh"
#include "GRadixSorter.hh"

template <typename REC> class GAIList {
  protected:
	static const int MAXC=10;
	struct AIRegData {
	    uint32_t start;   //region start: 0-based
	    uint32_t end;     //region end: not inclusive
	    REC data;      //  this could be pointers or an index into a storage array for data
	                   //  associated with each interval (e.g. GFF records)
	};
	struct AICtgData{
		char *name;  //name of contig
		int64_t nr, mr;  //number/capacity of glist
		AIRegData *glist;   //regions data
		int nc, lenC[MAXC], idxC[MAXC];  //components
		uint32_t *maxE;                  //augmentation
	};
	bool ready=false; //ready for query (only after build() is called)
	AICtgData *ctgs;            // list of contigs (of size nctg)
	int32_t nctg, mctg;   // count and max number of contigs
	//void *hc;              // dict for converting contig names to int
	GHashMap<const char*, int32_t>* ctghash;
	// hits data for the last query()
	uint32_t* hits;
	uint32_t h_cap;
	uint32_t h_count;
	int32_t  h_ctg; //index in ctg[] array
    //--methods
	static int32_t bSearch(AIRegData* As, int32_t idxS, int32_t idxE, uint32_t qe);
	int32_t getCtg(const char *chr) {
		int32_t* fi=this->ctghash->Find(chr);
		return fi ? *fi : -1;
	}
	void init() {
		ctghash=new GHashMap<const char*, int32_t>();
		ctghash->resize(64);
		nctg = 0;
		mctg = 32;
		ready=false;
		GMALLOC(ctgs, mctg*sizeof(AICtgData));
		h_count=0;
		h_cap=1000000;
		h_ctg=-1;
		GMALLOC(hits, h_cap*sizeof(uint32_t));
	}
	void destroy() {
		for (int i = 0; i < nctg; ++i){
			free(ctgs[i].name);
			free(ctgs[i].glist);
			free(ctgs[i].maxE);
		}
		free(ctgs);
		if (hits) free(hits);
		delete ctghash;
	}
  public:
	      //add a chr:start-end interval (0-based, end not included)
	void add(const char *chr, uint32_t s, uint32_t e, REC payload);
	void build(int cLen); //freeze the data, prepare for query
	//query a BED-style interval: 0 based, open-ended
	uint32_t query(const char *chr, uint32_t qs, uint32_t qe);
	uint32_t hitCount() { return h_count; } //for last query()
	REC hitData(uint32_t hit_idx) { //for last query()
		if (hit_idx<h_count && h_ctg>=0) {
			uint32_t h=hits[hit_idx];
			return ctgs[h_ctg].glist[h].data;
		}
	   return -1;
	}
	const char* hitCtg(uint32_t hit_idx) { //for last query()
		if (hit_idx<h_count && h_ctg>=0) {
			return ctgs[h_ctg].name;
		}
	   return NULL;
	}
	uint32_t hitStart(uint32_t hit_idx) { //for last query()
		if (hit_idx<h_count && h_ctg>=0) {
			uint32_t h=hits[hit_idx];
			return ctgs[h_ctg].glist[h].start;
		}
	   return (uint32_t)-1;
	}
	uint32_t hitEnd(uint32_t hit_idx) { //for last query()
		if (hit_idx<h_count && h_ctg>=0) {
			uint32_t h=hits[hit_idx];
			return ctgs[h_ctg].glist[h].end;
		}
	   return (uint32_t)-1;
	}

	const char* ctg(int32_t id) { if ((uint32_t)id<nctg) return ctgs[id].name; return NULL; }
	GAIList() { this->init(); }
	~GAIList() { this->destroy(); }
};

template<class REC> int32_t GAIList<REC>::bSearch(AIRegData* As, int32_t idxS, int32_t idxE, uint32_t qe) {   //find tE: index of the first item satisfying .s<qe from right
    int tE=-1, tL=idxS, tR=idxE-1, tM, d;
    uint32_t v;
    AIRegData* p=As;
    while((d=tR-tL)>1) {
    	tM = tL + (d>>1);
    	v=p[tM].start;
        if(v >= qe) tR = tM-1;
               else tL = tM;
    }
    if(p[tR].start < qe) tE = tR;
    else if(p[tL].start < qe)
                         tE = tL;
    return tE;
}

#define GA_REALLOC(ptr, len) ((ptr) = (__typeof__(ptr))realloc((ptr), (len) * sizeof(*(ptr))))
#define GA_EXPAND(a, m) { (m) = (m)? (m) + ((m)>>1) : 16; GA_REALLOC((a), (m)); }

template<class REC> void GAIList<REC>::add(const char *chr, uint32_t s, uint32_t e, REC payload) {
	if(s > e) return;
	bool cnew=false;
	uint64_t hidx=ctghash->addIfNew(chr, nctg, cnew);
	AICtgData *q;
	if (cnew) { //new contig
		if (nctg == mctg)
			{ GA_EXPAND(ctgs, mctg); }
		q = &ctgs[nctg++];
		q->name=strdup(chr);
		q->nr=0; q->mr=64;
		GMALLOC( q->glist, (q->mr * sizeof(AIRegData)) );
		ctghash->setKey(hidx, q->name);
	} else {
		q = &ctgs[ctghash->getValue(hidx)];
	}

	if (q->nr == q->mr)
		{ GA_EXPAND(q->glist, q->mr); }
	AIRegData *p = &q->glist[q->nr++];
	p->start = s;
	p->end   = e;
	p->data  = payload;
}

template<class REC> void GAIList<REC>::build(int cLen) {
	int cLen1=cLen/2, nr, minL = GMAX(64, cLen);
	cLen += cLen1;
	int lenT, len, iter, i, j, k, k0, t;
	GRadixSorter<AIRegData, uint32_t, & AIRegData::start> rdx;
	//KRadixSorter<AIRegData, uint32_t, & AIRegData::start> rdx;
	for(i=0; i<nctg; i++){
		//1. Decomposition
		AICtgData *p    = &ctgs[i];
		AIRegData *L1 = p->glist;  //L1: to be rebuilt
		nr = p->nr;
		//radix_sort_intv(L1, L1+nr);
		//radix_sort_regs(L1, L1+nr);
		//radix_sort(L1, nr);
		rdx.sort(L1, nr);
		if(nr<=minL){
			p->nc = 1, p->lenC[0] = nr, p->idxC[0] = 0;
		}
		else{
			AIRegData *L0 = (AIRegData *)malloc(nr*sizeof(AIRegData)); 	//L0: serve as input list
			AIRegData *L2 = (AIRegData *)malloc(nr*sizeof(AIRegData));   //L2: extracted list
			//----------------------------------------
			AIRegData *D0 = (AIRegData *)malloc(nr*sizeof(AIRegData)); 	//D0:
			int32_t *di = (int32_t*)malloc(nr*sizeof(int32_t));	//int64_t?
			//----------------------------------------
			memcpy(L0, L1, nr*sizeof(AIRegData));
			iter = 0;	k = 0;	k0 = 0;
			lenT = nr;
			while(iter<MAXC && lenT>minL){
				//setup di---------------------------
				for(j=0;j<lenT;j++){				//L0:{.start= end, .end=idx, .value=idx1}
					D0[j].start = L0[j].end;
					D0[j].end = j;
				}
				//radix_sort_intv(D0, D0+lenT);
				//radix_sort_regs(D0, D0+lenT);
				//radix_sort6(D0,lenT);
				rdx.sort(D0, lenT);
				for(j=0;j<lenT;j++){				//assign i=29 to L0[i].end=2
					t = D0[j].end;
					di[t] = j-t;					//>0 indicate containment
				}
				//-----------------------------------
				len = 0;
				for(t=0;t<lenT-cLen;t++){
					if(di[t]>cLen)
						memcpy(&L2[len++], &L0[t], sizeof(AIRegData));
					else
						memcpy(&L1[k++], &L0[t], sizeof(AIRegData));
				}
				memcpy(&L1[k], &L0[lenT-cLen], cLen*sizeof(AIRegData));
				k += cLen, lenT = len;
				p->idxC[iter] = k0;
				p->lenC[iter] = k-k0;
				k0 = k, iter++;
				if(lenT<=minL || iter==MAXC-2){			//exit: add L2 to the end
					if(lenT>0){
						memcpy(&L1[k], L2, lenT*sizeof(AIRegData));
						p->idxC[iter] = k;
						p->lenC[iter] = lenT;
						iter++;
						lenT = 0;						//exit!
					}
					p->nc = iter;
				}
				else memcpy(L0, L2, lenT*sizeof(AIRegData));
			}
			free(L2),free(L0), free(D0), free(di);
		}
		//2. Augmentation
		p->maxE = (uint32_t*)malloc(nr*sizeof(uint32_t));
		for(j=0; j<p->nc; j++){
			k0 = p->idxC[j];
			k = k0 + p->lenC[j];
			uint32_t tt = L1[k0].end;
			p->maxE[k0]=tt;
			for(t=k0+1; t<k; t++){
				if(L1[t].end > tt) tt = L1[t].end;
				p->maxE[t] = tt;
			}
		}
	}
	this->ready=true;
}

template<class REC> uint32_t GAIList<REC>::query(const char *chr, uint32_t qs, uint32_t qe) {
    //interestingly enough having a local copy of the hits-related fields
    // leads to better speed optimization! (likely due to using registers?)
    uint32_t nr = 0, m = h_cap, newc;
    uint32_t* r = hits;
    if (!this->ready) { fprintf(stderr, "Error: GAIList query() call before build()!\n"); exit(1); }
    int32_t gid = getCtg(chr);
    if(gid>=nctg || gid<0) { h_ctg=-1; h_count=0; return 0;}
    AICtgData *p = &ctgs[gid];
    for(int k=0; k<p->nc; k++) { //search each component
        int32_t cs = p->idxC[k];
        int32_t ce = cs + p->lenC[k];
        int32_t t;
        if(p->lenC[k]>15){ //use binary search
            t = bSearch(p->glist, cs, ce, qe); 	//rs<qe: inline not better
            if(t>=cs){
            	newc=nr+t-cs;
		        if(newc>=m){
		        	m = newc + 1024;
		        	r = (uint32_t*)realloc(r, m*sizeof(uint32_t));
		        }
		        while(t>=cs && p->maxE[t]>qs){
		            if(p->glist[t].end>qs)
		                r[nr++] = t;
		            t--;
		        }
            }
        }
        else { //use linear search
        	newc=nr+ce-cs;
        	if(newc>=m){
        		m = newc + 1024;
        		r = (uint32_t*)realloc(r, m*sizeof(uint32_t));
        	}
            for(t=cs; t<ce; t++)
                if(p->glist[t].start<qe && p->glist[t].end>qs)
                    r[nr++] = t;
        }
    }
    //update back the hits-related fields:
    hits = r, h_cap=m, h_count=nr;
    h_ctg = gid;
    return nr;
 }

#endif
