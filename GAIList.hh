//=====================================================================================
//Common structs, parameters, functions
//by Jianglin Feng  09/5/2018
//-------------------------------------------------------------------------------------

#ifndef __GAILIST_H__
#define __GAILIST_H__
//-------------------------------------------------------------------------------------
#include "GHashMap.hh"
#include "GRadixSorter.hh"
#include <vector>
/*
  struct AICtgData{
		char *name;  //name of contig
		int64_t nr, mr;  //number/capacity of glist
		AIRegData *glist;   //regions data
		int nc, lenC[MAXC], idxC[MAXC];  //components
		uint32_t *maxE;                  //augmentation
	};
	AICtgData *ctgs;            // list of contigs (of size nctg)
	int32_t nctg, mctg;   // count and max number of contigs
	GHashMap<const char*, int32_t>* ctghash;
	bool ready=false; //ready for query (only after build() is called)

	// hits data for the last query()
	uint32_t* hits;
	uint32_t h_cap;
	uint32_t h_count;
    int32_t  h_ctg; //index in ctg[] array


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
	int32_t getCtg(const char *chr) {
		int32_t* fi=this->ctghash->Find(chr);
		return fi ? *fi : -1;
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

	//add & query BED-style intervals: 0 based, open-ended
	void add(const char *chr, uint32_t s, uint32_t e, REC payload);
	uint32_t query(const char *chr, uint32_t qs, uint32_t qe);

 */

//one GAIList per contig!

template <typename REC> class GAIList {
  public:
	struct AIRegData {
	    uint32_t start;   //region start: 0-based
	    uint32_t end;     //region end: not inclusive
	    REC data;      //  this could be pointers or an index into a storage array for data
	                   //  associated with each interval (e.g. GFF records)
	};
  protected:
	static const int MAXC=10;
	//these are all per contig
	int64_t nr, mr;  //number/capacity of glist
	AIRegData *glist;   //regions data
	int nc, lenC[MAXC], idxC[MAXC];  //components
	uint32_t *maxE;                  //augmentation

    //--methods
	static int32_t bSearch(AIRegData* As, int32_t idxS, int32_t idxE, uint32_t qe);

	void init() {
		nr=0; mr=64; nc=0; maxE=NULL;
		GMALLOC(glist, (mr * sizeof(AIRegData)) );
	}
	void destroy() {
		free(glist);
		free(maxE);
	}
  public:
    AIRegData& getReg(int64_t hidx) { return glist[hidx]; } //no check!
   //add a chr:start-end interval (0-based, end not included)
	void add(uint32_t s, uint32_t e, REC payload);
	void build(int cLen); //freeze the data, prepare for query
	//query a BED-style interval: 0 based, open-ended
	//uint32_t query(const char *chr, uint32_t qs, uint32_t qe);
	uint32_t query(uint32_t qs, uint32_t qe, uint32_t& h_cap, uint32_t* & hits);
	//const char* ctg(int32_t id) { if ((uint32_t)id<nctg) return ctgs[id].name; return NULL; }
	GAIList() { this->init(); }
	~GAIList() { this->destroy(); }
};


template <typename REC> class GAIListSet {
	//for all chromosomes, also holds results
  protected:
	struct AICtgData{
			char *name;  //name of contig
			GAIList<REC> ailst; //interval list for each contig
			AICtgData(const char* ctg=NULL):name(NULL), ailst() {
				 //ctg must be duplicated when not sharing strings
				 name=const_cast<char *>(ctg); 
			}

			//~AICtgData() { free(name); }
	};
	// hits data for the last query()
	uint32_t* hits;
	uint32_t h_cap;
	uint32_t h_count;
    int32_t  h_ctg; //index in ctg[] array
	bool shCtgNames;
  public:
	GDynArray<AICtgData*> ctgs;            // collection of contigs (of size nctg)
	//int32_t nctg, mctg;   // count and max number of contigs
	GHashMap<const char*, int32_t>* ctghash;
	bool ready=false; //ready for query (only after build() was called)

    GAIListSet(bool externalContigNames=false):hits(NULL), h_cap(1000000), h_count(0), 
	 		h_ctg(-1), shCtgNames(externalContigNames),ctgs(32) {
	    ctghash=new GHashMap<const char*, int32_t>();
		ctghash->resize(64);
		ready=false;
		GMALLOC(hits, h_cap*sizeof(uint32_t));
	}
	~GAIListSet() {
		for (uint i=0; i < ctgs.Count();++i) {
			if (!shCtgNames) free(ctgs[i]->name);
			delete ctgs[i];
		}
		if (hits) free(hits);
		delete ctghash;
	}

	int32_t getCtg(const char *chr) {
		int32_t* fi=this->ctghash->Find(chr);
		return fi ? *fi : -1;
	}

	void build(int cLen) {
		for(uint i=0; i<ctgs.Count(); i++)
			ctgs[i]->ailst.build(cLen);
	}

	//add & query BED-style intervals: 0 based, open-ended
	void add(const char *chr, uint32_t s, uint32_t e, REC payload) {
		if(s > e) return;
		bool cnew=false; //new contig
		int nctg=ctgs.Count();
		uint64_t hidx=ctghash->addIfNew(chr, nctg, cnew);
		AICtgData *q;
		if (cnew) {
		  q=new AICtgData(chr);
		  ctgs.Add(q);
		  if (!shCtgNames) {
			 q->name=strdup(chr);
		     ctghash->setKey(hidx, q->name);
		  }
		}
		else q=ctgs[ctghash->getValue(hidx)];
		q->ailst.add(s,e,payload);
	}

	uint32_t query(const char *chr, uint32_t qs, uint32_t qe) {
	    int32_t gid = getCtg(chr);
	    if ((uint32_t)gid>=ctgs.Count()) { h_ctg=-1; h_count=0; return 0;}
	    h_ctg=gid;
	    h_count=ctgs[gid]->ailst.query(qs, qe, this->h_cap, this->hits);
	    return h_count;
	}

	uint32_t hitCount() { return h_count; } //for last query()
	REC hitData(uint32_t hit_idx) { //for last query()
		if (hit_idx<h_count && h_ctg>=0) {
			uint32_t h=hits[hit_idx];
			return ctgs[h_ctg]->ailst.getReg(h).data;
		}
	   return -1;
	}
	const char* hitCtg(uint32_t hit_idx) { //for last query()
		if (hit_idx<h_count && h_ctg>=0) {
			return ctgs[h_ctg]->name;
		}
	   return NULL;
	}
	uint32_t hitStart(uint32_t hit_idx) { //for last query()
		if (hit_idx<h_count && h_ctg>=0) {
			uint32_t h=hits[hit_idx];
			return ctgs[h_ctg]->ailst.getReg(h).start;
		}
	   return (uint32_t)-1;
	}
	uint32_t hitEnd(uint32_t hit_idx) { //for last query()
		if (hit_idx<h_count && h_ctg>=0) {
			uint32_t h=hits[hit_idx];
			return ctgs[h_ctg]->ailst.getReg(h).end;
		}
	   return (uint32_t)-1;
	}

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

//template<class REC> void GAIList<REC>::add(const char *chr, uint32_t s, uint32_t e, REC payload) {
template<class REC> void GAIList<REC>::add(uint32_t s, uint32_t e, REC payload) {
	if (nr == mr)
		{ GA_EXPAND(glist, mr); }
	AIRegData *p = &glist[nr++];
	p->start = s;
	p->end   = e;
	p->data  = payload;
}

template<class REC> void GAIList<REC>::build(int cLen) {
	int cLen1=cLen/2, minL = GMAX(64, cLen);
	cLen += cLen1;
	int lenT, len, iter, j, k, k0, t;
	GRadixSorter<AIRegData, uint32_t, & AIRegData::start> rdx;
	//for(i=0; i<nctg; i++){nctg
	//1. Decomposition
		AIRegData *L1 = glist;  //L1: to be rebuilt
		rdx.sort(L1, nr);
		if(nr<=minL) {
			nc = 1, lenC[0] = nr, idxC[0] = 0;
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
				idxC[iter] = k0;
				lenC[iter] = k-k0;
				k0 = k, iter++;
				if(lenT<=minL || iter==MAXC-2){			//exit: add L2 to the end
					if(lenT>0){
						memcpy(&L1[k], L2, lenT*sizeof(AIRegData));
						idxC[iter] = k;
						lenC[iter] = lenT;
						iter++;
						lenT = 0;						//exit!
					}
					nc = iter;
				}
				else memcpy(L0, L2, lenT*sizeof(AIRegData));
			}
			free(L2),free(L0), free(D0), free(di);
		}
		//2. Augmentation
		maxE = (uint32_t*)malloc(nr*sizeof(uint32_t));
		for(j=0; j<nc; j++){
			k0 = idxC[j];
			k = k0 + lenC[j];
			uint32_t tt = L1[k0].end;
			maxE[k0]=tt;
			for(t=k0+1; t<k; t++){
				if(L1[t].end > tt) tt = L1[t].end;
				maxE[t] = tt;
			}
		}
	//} for each contig
	//this->ready=true;
}

//template<class REC> uint32_t GAIList<REC>::query(const char *chr, uint32_t qs, uint32_t qe) {
template<class REC> uint32_t GAIList<REC>::query(uint32_t qs, uint32_t qe, uint32_t& h_cap, uint32_t* & hits) {
    //interestingly enough having a local copy of the hits-related fields
    // leads to better speed optimization! (likely due to using registers?)
    uint32_t nres = 0, newc, m = h_cap;
    uint32_t* r = hits;
    /*
    if (!this->ready) { fprintf(stderr, "Error: GAIList query() call before build()!\n"); exit(1); }
    int32_t gid = getCtg(chr);
    if(gid>=nctg || gid<0) { h_ctg=-1; h_count=0; return 0;}
    AICtgData *p = &ctgs[gid];
    */
    for(int k=0; k<nc; k++) { //search each component
        int32_t cs = idxC[k];
        int32_t ce = cs + lenC[k];
        int32_t t;
        if(lenC[k]>15){ //use binary search
            t = bSearch(glist, cs, ce, qe); 	//rs<qe: inline not better
            if(t>=cs){
            	newc=nres+t-cs;
		        if(newc>=m){
		        	m = newc + 1024;
		        	r = (uint32_t*)realloc(r, m*sizeof(uint32_t));
		        }
		        while(t>=cs && maxE[t]>qs){
		            if(glist[t].end>qs)
		                r[nres++] = t;
		            t--;
		        }
            }
        }
        else { //use linear search
        	newc=nres+ce-cs;
        	if(newc>=m){
        		m = newc + 1024;
        		r = (uint32_t*)realloc(r, m*sizeof(uint32_t));
        	}
            for(t=cs; t<ce; t++)
                if(glist[t].start<qe && glist[t].end>qs)
                    r[nres++] = t;
        }
    }
    //update back the hits-related fields:
    hits = r, h_cap=m; //h_count=nres;
    //h_ctg = gid;
    return nres;
 }

#endif
