#include "GBase.h"
#include "GArgs.h"
#include "GStr.h"
#include "GVec.hh"
#include "GHash.hh"
#include "GIntHash.hh"
#include "GResUsage.h"

#define USAGE "Usage:\n\
  rusage <inputfile> \n\
 Resource usage testing, loading the given file <inputfile>.\n\
 "
int main(int argc, char* argv[]) {
	GArgs args(argc, argv, "h");
	int numargs=args.startNonOpt();
	if (args.getOpt('h') || numargs!=1) {
		GMessage("%s\n",USAGE);
		exit(0);
	}

	char* mdstr=args.nextNonOpt();
	//--make a copy of the string, in case the original is a const string
	// (because the parseUInt() function modifies the string temporarily
	char* mdstring=Gstrdup(mdstr);
	char *p=mdstring;

	while (*p!='\0') {
		int num_matches=0;
		if (parseInt(p,num_matches)) {
			if (num_matches!=0)
				GMessage("%d matching bases\n", num_matches);
			continue;
		}
		if (*p=='^') { //deletion
			GDynArray<char> deletion; //deletion string accumulates here (if needed)
			int del_length=0;//tracking deletion length
			char delbase=*(++p);
			while (delbase>='A' && delbase<='Z') {
				deletion.Add(delbase);
				del_length++;
				delbase=*(++p);
			}
			GMessage("%d base(s) deletion [", del_length);
			for (uint i=0;i<deletion.Count();++i) GMessage("%c",deletion[i]);
			GMessage("]\n");
			continue;
		}
		if (*p>='A' && *p<='Z') {
			GMessage("base mismatch [%c]\n",*p);
			p++;
			continue;
		}
		GMessage("Warning: skipping unrecognized char [%c]\n", *p);
		p++;
	}

	GFREE(mdstring);
	return 0;
}
