#include "GBase.h"
#include "GArgs.h"
#include "GStr.h"
#include "GVec.hh"
#include "GHash.hh"
#include "GResUsage.h"
#include <cstdint>
#include <iostream>
#include "tsl/hopscotch_map.h"
#include "tsl/robin_map.h"
//#include "ska/bytell_hash_map.hpp"

//#include "khashl.hh"
//#include "city.h"
#include "GKHash.hh"

#define USAGE "Usage:\n\
  htest [-Q] [-n num_clusters] textfile.. \n\
  \n\
 "

bool qryMode=false;
int numClusters=500;


static void strFreeProc(pointer item) {
      GFREE(item);
}

struct HStrData {
	int cmd; // 0=add, 1=remove, 2=clear
	GStr str;
	HStrData(char* s=NULL, int c=0):cmd(c), str(s) { }
};

int loadStrings(FILE* f, GPVec<HStrData>& strgsuf, GPVec<HStrData>& strgs, int toLoad) {
  int num=0;
  GLineReader lr(f);
  char* line=NULL;
  int numcl=0;
  while ((line=lr.nextLine())!=NULL) {
	  int len=strlen(line);
	  if (len<3) continue;
	  if (line[0]=='>') {
		  numcl++;
		  if (toLoad && numcl>toLoad) {
			  break;
		  }
		  continue;
	  }
	  if (strcmp(line, "HCLR")==0) {
		  strgs.Add(new HStrData(NULL, 2));
		  strgsuf.Add(new HStrData(NULL, 2));
		  continue;
	  }
	  if (startsWith(line, "RM ")) {
	     strgsuf.Add(new HStrData(line+3,1) );
	     line[len-3]=0;
	     strgs.Add(new HStrData(line+3,1));
	     continue;
	  }
      strgsuf.Add(new HStrData(line));
      line[len-3]=0;
      strgs.Add(new HStrData(line));
	  num++;
  } //while line
  return num;
}

void showTimings(GResUsage swatch) {
 char *wtime=commaprintnum((uint64_t)swatch.elapsed());
 char *utime=commaprintnum((uint64_t)swatch.u_elapsed());
 char *stime=commaprintnum((uint64_t)swatch.s_elapsed());
 char *smem=commaprintnum((uint64_t)swatch.memoryUsed());
 GMessage("Elapsed time (microseconds): %12s us\n", wtime);
 GMessage("                  user time: %12s us\n", utime);
 GMessage("                system time: %12s us\n", stime);
 GMessage("                  mem usage: %12s KB\n", smem);

 GFREE(wtime);GFREE(utime);GFREE(stime); GFREE(smem);
}


// default values recommended by http://isthe.com/chongo/tech/comp/fnv/
const uint32_t Prime = 0x01000193; // 16777619
const uint32_t Seed  = 0x811C9DC5; // 2166136261
/// hash a single byte
inline uint32_t fnv1a(unsigned char b, uint32_t h = Seed) {   return (b ^ h) * Prime; }

/// hash a C-style string
uint32_t fnv1a(const char* text, uint32_t hash = Seed) {
	while (*text)
		hash = fnv1a((unsigned char)*text++, hash);
	return hash;
}

struct cstr_eq {
    inline bool operator()(const char* x, const char* y) const {
        return (strcmp(x, y) == 0);
    }
};

struct cstr_hash {
	inline uint32_t operator()(const char* s) const {
		return CityHash32(s, std::strlen(s));
		//return fnv1a(s);
	}
};


void run_GHash(GResUsage& swatch, GPVec<HStrData> & hstrs, const char* label) {
	GHash<int> ghash;
	int num_add=0, num_rm=0, num_clr=0;
	GMessage("----------------- %s ----------------\n", label);
	ghash.Clear();
	swatch.start();
	int cl_i=0;
	int prevcmd=2;
	for (int i=0;i<hstrs.Count();i++) {
			  if (hstrs[i]->cmd==prevcmd) {
				  if (prevcmd==2) continue;
			  } else prevcmd=hstrs[i]->cmd;
			  switch (hstrs[i]->cmd) {
				case 0:
					if (cl_i==0) cl_i=i;
					ghash.fAdd(hstrs[i]->str.chars(), new int(i));
					num_add++;
					break;
				case 1:
					if (qryMode) break;
					ghash.Remove(hstrs[i]->str.chars());
					num_rm++;
					break;
				case 2:
					//run tests here
					if (qryMode) {
						//run some query tests here
						for(int j=cl_i;j<i;j+=3) {
							if (hstrs[j]->cmd) continue;
							int* v=ghash[hstrs[j]->str.chars()];
							if (v==NULL)
								GError("Error at <%s>, key %s not found (count:%d, cl_i=%d, i=%d)!\n",label, hstrs[j]->str.chars(),
										ghash.Count(), cl_i, i );
							if (*v!=j)
								GError("Error at <%s>, invalid value for key %s!\n",label, hstrs[j]->str.chars() );
						}
					}
					cl_i=0;
					ghash.Clear();
					num_clr++;
					break;
			  }
	}
	swatch.stop();
	ghash.Clear();
	GMessage("  (%d inserts, %d deletions, %d clears)\n", num_add, num_rm, num_clr);
}

void run_Hopscotch(GResUsage& swatch, GPVec<HStrData> & hstrs, const char* label) {
  int num_add=0, num_rm=0, num_clr=0;
  //tsl::hopscotch_map<const char*, int, cstr_hash, cstr_eq> hsmap;
  tsl::hopscotch_map<const char*, int, cstr_hash, cstr_eq,
      std::allocator<std::pair<const char*, int>>,  30, true> hsmap;
  GMessage("----------------- %s ----------------\n", label);
  swatch.start();
  int cl_i=0;
  int prevcmd=2;
  for (int i=0;i<hstrs.Count();i++) {
	  if (hstrs[i]->cmd==prevcmd) {
		  if (prevcmd==2) continue;
	  } else prevcmd=hstrs[i]->cmd;
	  switch (hstrs[i]->cmd) {
		case 0:
			if (cl_i==0) cl_i=i;
			hsmap.insert({hstrs[i]->str.chars(), i});
			num_add++;
			break;
		case 1:
			if (qryMode) break;
			hsmap.erase(hstrs[i]->str.chars());
			num_rm++; break;
		case 2:
			if (qryMode) {
				//run some query tests here
				//with strings from hstrs[cl_i .. i-1] range
				for(int j=cl_i;j<i;j+=3) {
					if (hstrs[j]->cmd) continue;
					int v=hsmap[hstrs[j]->str.chars()];
					if (v!=j)
						GError("Error at <%s>, invalid value for key %s! (got %d, expected %d)\n",label,
								hstrs[j]->str.chars(), v, j );
				}
			}
			cl_i=0;
			hsmap.clear();
			num_clr++;
			break;
	  }
  }
  swatch.stop();
  hsmap.clear();
  GMessage("  (%d inserts, %d deletions, %d clears)\n", num_add, num_rm, num_clr);
}

void run_Robin(GResUsage& swatch, GPVec<HStrData> & hstrs, const char* label) {
  int num_add=0, num_rm=0, num_clr=0;
  //tsl::hopscotch_map<const char*, int, cstr_hash, cstr_eq> hsmap;
  tsl::robin_map<const char*, int, cstr_hash, cstr_eq,
      std::allocator<std::pair<const char*, int>>,  true> rmap;
  GMessage("----------------- %s ----------------\n", label);
  swatch.start();
  int cl_i=0;
  int prevcmd=2;
  for (int i=0;i<hstrs.Count();i++) {
	  if (hstrs[i]->cmd==prevcmd) {
		  if (prevcmd==2) continue;
	  } else prevcmd=hstrs[i]->cmd;
	  switch (hstrs[i]->cmd) {
		case 0:
			if (cl_i==0) cl_i=i;
			rmap.insert({hstrs[i]->str.chars(), i});
			num_add++;
			break;
		case 1: if (qryMode) break;
			rmap.erase(hstrs[i]->str.chars()); num_rm++; break;
		case 2:
			if (qryMode) {
				//run some query tests here
				//with strings from hstrs[cl_i .. i-1] range
				for(int j=cl_i;j<i;j+=3) {
					if (hstrs[j]->cmd) continue;
					int v=rmap[hstrs[j]->str.chars()];
					if (v!=j)
						GError("Error at <%s>, invalid value for key %s!\n",label, hstrs[j]->str.chars() );
				}
			}
			cl_i=0;
			rmap.clear(); num_clr++; break;
	  }
  }
  swatch.stop();
  rmap.clear();
  GMessage("  (%d inserts, %d deletions, %d clears)\n", num_add, num_rm, num_clr);
}
/*
void run_Bytell(GResUsage& swatch, GPVec<HStrData> & hstrs, const char* label) {
  int num_add=0, num_rm=0, num_clr=0;
  ska::bytell_hash_map<const char*, int, cstr_hash, cstr_eq> bmap;
  GMessage("----------------- %s ----------------\n", label);
  swatch.start();
  for (int i=0;i<hstrs.Count();i++) {
	  switch (hstrs[i]->cmd) {
		case 0:bmap.insert({hstrs[i]->str.chars(), 1}); num_add++; break;
		case 1:bmap.erase(hstrs[i]->str.chars()); num_rm++; break;
		case 2:bmap.clear(); num_clr++; break;
	  }
  }
  swatch.stop();
  bmap.clear();
  GMessage("  (%d inserts, %d deletions, %d clears)\n", num_add, num_rm, num_clr);
}
*/
void run_Khashl(GResUsage& swatch, GPVec<HStrData> & hstrs, const char* label) {
  int num_add=0, num_rm=0, num_clr=0;
  klib::KHashMapCached<const char*, int, cstr_hash, cstr_eq > khmap;
  GMessage("----------------- %s ----------------\n", label);
  swatch.start();
  int cl_i=0;
  int prevcmd=2;
  for (int i=0;i<hstrs.Count();i++) {
	  if (hstrs[i]->cmd==prevcmd) {
		  if (prevcmd==2) continue;
	  } else prevcmd=hstrs[i]->cmd;
	  switch (hstrs[i]->cmd) {
		case 0:if (cl_i==0) cl_i=i;
			khmap[hstrs[i]->str.chars()]=i; num_add++; break;
		case 1:if (qryMode) break;
			khmap.del(khmap.get(hstrs[i]->str.chars())); num_rm++; break;
		case 2:
			if (qryMode) {
				//run some query tests here
				for(int j=cl_i;j<i;j+=3) {
					if (hstrs[j]->cmd) continue;
					int v=khmap[hstrs[j]->str.chars()];
					if (v!=j)
						GError("Error at <%s>, invalid value for key %s!\n",label, hstrs[j]->str.chars() );
				}
			}
			cl_i=0;
			khmap.clear(); num_clr++; break;
	  }
  }
  swatch.stop();
  khmap.clear();
  GMessage("  (%d inserts, %d deletions, %d clears)\n", num_add, num_rm, num_clr);
}

void run_GKHashSet(GResUsage& swatch, GPVec<HStrData> & hstrs, const char* label) {
  int num_add=0, num_rm=0, num_clr=0;
  GKHashPtrSet<char> khset;
  GMessage("----------------- %s ----------------\n", label);
  int cl_i=0;
  swatch.start();
  int prevcmd=2;
  for (int i=0;i<hstrs.Count();i++) {
	  if (hstrs[i]->cmd==prevcmd) {
		  if (prevcmd==2) continue;
	  } else prevcmd=hstrs[i]->cmd;
	  switch (hstrs[i]->cmd) {
		case 0: if (cl_i==0) cl_i=i;
			khset.Add(hstrs[i]->str.chars()); num_add++; break;
		case 1:if (qryMode) break;
			khset.Remove(hstrs[i]->str.chars()); num_rm++; break;
		case 2:
			if (qryMode) {
				//run some query tests here
				//with strings from hstrs[cl_i .. i-1] range
				for(int j=cl_i;j<i;j+=3) {
					if (hstrs[j]->cmd) continue;
					if (!khset[hstrs[j]->str.chars()])
						GError("Error at <%s>, invalid value for key %s!\n",label, hstrs[j]->str.chars() );
				}
			}
			cl_i=0;
			khset.Clear(); num_clr++; break;
	  }
  }
  swatch.stop();
  khset.Clear();
  GMessage("  (%d inserts, %d deletions, %d clears)\n", num_add, num_rm, num_clr);
}

void run_GKHashSetShk(GResUsage& swatch, GPVec<HStrData> & hstrs, const char* label) {
  int num_add=0, num_rm=0, num_clr=0;
  GKHashPtrSet<char> khset;
  GMessage("----------------- %s ----------------\n", label);
  int cl_i=0;
  swatch.start();
  int prevcmd=2;
  for (int i=0;i<hstrs.Count();i++) {
	  if (hstrs[i]->cmd==prevcmd) {
		  if (prevcmd==2) continue;
	  } else prevcmd=hstrs[i]->cmd;
	  switch (hstrs[i]->cmd) {
		case 0: if (cl_i==0) cl_i=i;
			khset.shkAdd(hstrs[i]->str.chars()); num_add++; break;
		case 1:if (qryMode) break;
			khset.Remove(hstrs[i]->str.chars()); num_rm++; break;
		case 2:
			if (qryMode) {
				//run some query tests here
				//with strings from hstrs[cl_i .. i-1] range
				for(int j=cl_i;j<i;j+=3) {
					if (hstrs[j]->cmd) continue;
					if (!khset[hstrs[j]->str.chars()])
						GError("Error at <%s>, invalid value for key %s!\n",label, hstrs[j]->str.chars() );
				}
			}
			cl_i=0;
			khset.Clear(); num_clr++; break;
	  }
  }
  swatch.stop();
  khset.Clear();
  GMessage("  (%d inserts, %d deletions, %d clears)\n", num_add, num_rm, num_clr);
}


int main(int argc, char* argv[]) {
 GPVec<HStrData> strs;
 GPVec<HStrData> sufstrs;
 strs.setFreeItem(strFreeProc);
 sufstrs.setFreeItem(strFreeProc);
 //GArgs args(argc, argv, "hg:c:s:t:o:p:help;genomic-fasta=COV=PID=seq=out=disable-flag;test=");
 GArgs args(argc, argv, "hQn:");
 //fprintf(stderr, "Command line was:\n");
 //args.printCmdLine(stderr);
 args.printError(USAGE, true);
 if (args.getOpt('h') || args.getOpt("help")) GMessage(USAGE);
 GStr s=args.getOpt('n');
 if (!s.is_empty()) {
	 numClusters=s.asInt();
	 if (numClusters<=0)
		 GError("%s\nError: invalid value for -n !\n", USAGE);
 }
 qryMode=(args.getOpt('Q'));
 int numargs=args.startNonOpt();
 const char* a=NULL;
 FILE* f=NULL;
 int total=0;

 if (numargs==0) {
	 //a="htest_data.lst";
	 a="htest_over500.lst";
	 f=fopen(a, "r");
	 if (f==NULL) GError("Error: could not open file %s !\n", a);
	 GMessage("loading %d clusters from file..\n", numClusters);
	 int num=loadStrings(f, sufstrs, strs, numClusters);
	 total+=num;
 }
 else {
	   while ((a=args.nextNonOpt())) {
		   f=fopen(a, "r");
		   if (f==NULL) GError("Error: could not open file %s !\n", a);
		   int num=loadStrings(f, sufstrs, strs, numClusters);
		   total+=num;
	   }
  }
   GResUsage swatch;


   GMessage("size of std::string = %d, of GStr = %d\n", sizeof(std::string), sizeof(GStr));
   //run_GHash(swatch, strs, "GHash no suffix");
   //showTimings(swatch);

   run_GHash(swatch, sufstrs, "GHash w/ suffix");
   showTimings(swatch);


   run_Hopscotch(swatch, sufstrs, "hopscotch w/ suffix");
   showTimings(swatch);
   run_Hopscotch(swatch, strs, "hopscotch no suffix");
   showTimings(swatch);


   run_Robin(swatch, sufstrs, "robin w/ suffix");
   showTimings(swatch);
   run_Robin(swatch, strs, "robin no suffix");
   showTimings(swatch);

   run_Khashl(swatch, sufstrs, "khashl w/ suffix");
   showTimings(swatch);
   run_Khashl(swatch, strs, "khashl no suffix");
   showTimings(swatch);

   run_GKHashSet(swatch, sufstrs, "GKHashSet w/ suffix");
   showTimings(swatch);
   run_GKHashSet(swatch, strs, "GKHashSet no suffix");
   showTimings(swatch);

   run_GKHashSetShk(swatch, sufstrs, "GKHashSetShk w/ suffix");
   showTimings(swatch);
   run_GKHashSetShk(swatch, strs, "GKHashSetShk no suffix");
   showTimings(swatch);

/*
   run_Bytell(swatch, sufstrs, "bytell w/ suffix");
   showTimings(swatch);
   run_Bytell(swatch, strs, "bytell no suffix");
   showTimings(swatch);
*/

}
