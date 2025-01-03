HTSLIB_PREFIX := .    
## can be replaced with a pre-installed prefix
## otherwise this script will install it there

HTSLIB_PREFIX := $(strip $(HTSLIB_PREFIX))
HTSLIB := $(HTSLIB_PREFIX)/lib/libhts.a
HTSINC := $(HTSLIB_PREFIX)/include

INCDIRS := -I$(HTSINC)


CXX   := $(if $(CXX),$(CXX),g++)
LINKER  := $(if $(LINKER),$(LINKER),g++)
LDFLAGS := $(if $(LDFLAGS),$(LDFLAGS),-g -L $(HTSLIB_PREFIX)/lib)

HTS_XLIBS := -lbz2 -llzma
LIBS := $(HTSLIB) -lz -lm $(HTS_XLIBS) -lpthread



DMACH := $(shell $(CXX) -dumpmachine)

ifneq (, $(findstring mingw32, $(DMACH)))
 WINDOWS=1
endif

ifneq (, $(findstring linux, $(DMACH)))
 # -lrt only needed for Linux systems
 LIBS+= -lrt
endif

# MinGW32 GCC 4.5 link problem fix
ifdef WINDOWS
 DMACH := windows
 ifeq ($(findstring 4.5.,$(shell $(CXX) -dumpversion)), 4.5.)
  LDFLAGS += -static-libstdc++ -static-libgcc
 endif
  LIBS += -lregex -lws2_32
endif

# Misc. system commands
# assuming on Windows this is run under MSYS
RM = /usr/bin/rm -f

# File endings
ifdef WINDOWS
EXE = .exe
else
EXE =
endif

BASEFLAGS  := -std=c++11 -Wall -Wextra $(INCDIRS) $(MARCH) \
 -D_REENTRANT -fno-exceptions -fno-rtti

GCCVER5 := $(shell expr `${CXX} -dumpversion | cut -f1 -d.` \>= 5)
ifeq "$(GCCVER5)" "1"
 BASEFLAGS += -Wno-implicit-fallthrough
endif

GCCVER8 := $(shell expr `${CXX} -dumpversion | cut -f1 -d.` \>= 8)
ifeq "$(GCCVER8)" "1"
  BASEFLAGS += -Wno-class-memaccess
endif

#add the link-time optimization flag if gcc version > 4.5
GCC_VERSION:=$(subst ., ,$(shell gcc -dumpversion))
GCC_MAJOR:=$(word 1,$(GCC_VERSION))
GCC_MINOR:=$(word 2,$(GCC_VERSION))
#GCC_SUB:=$(word 3,$(GCC_VERSION))
GCC_SUB:=x

ifneq (,$(filter %release %nodebug, $(MAKECMDGOALS)))
  # -- release build
  CXXFLAGS := $(if $(CXXFLAGS),$(CXXFLAGS),-g -O3)
  CXXFLAGS += -DNDEBUG $(BASEFLAGS)
else
  ifneq (,$(filter %memcheck %tsan, $(MAKECMDGOALS)))
     #use sanitizer in gcc 4.9+
     GCCVER49 := $(shell expr `${CXX} -dumpversion | cut -f1,2 -d.` \>= 4.9)
     ifeq "$(GCCVER49)" "0"
       $(error gcc version 4.9 or greater is required for this build target)
     endif
     CXXFLAGS := $(if $(CXXFLAGS),$(CXXFLAGS),-g -O0)
     SANLIBS :=
     ifneq (,$(filter %tsan %tcheck %thrcheck, $(MAKECMDGOALS)))
        # thread sanitizer only (incompatible with address sanitizer)
        CXXFLAGS += -fno-omit-frame-pointer -fsanitize=thread -fsanitize=undefined $(BASEFLAGS)
        SANLIBS := -ltsan
     else
        # address sanitizer
        CXXFLAGS += -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=address $(BASEFLAGS)
        SANLIBS := -lasan
     endif
     ifeq "$(GCCVER5)" "1"
       CXXFLAGS += -fsanitize=bounds -fsanitize=float-divide-by-zero -fsanitize=vptr
       CXXFLAGS += -fsanitize=float-cast-overflow -fsanitize=object-size
       #CXXFLAGS += -fcheck-pointer-bounds -mmpx
     endif
     CXXFLAGS += -DDEBUG -D_DEBUG -DGDEBUG -fno-common -fstack-protector
     LIBS := $(SANLIBS) -lubsan -ldl $(LIBS)
   else
   # -- plain debug build
    CXXFLAGS := $(if $(CXXFLAGS),$(CXXFLAGS), -O0 -g)
    ifneq (, $(findstring darwin, $(DMACH)))
        CXXFLAGS += -gdwarf-3
    endif
    CXXFLAGS += -DDEBUG -D_DEBUG -DGDEBUG $(BASEFLAGS)
   endif
endif

all: gtest

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(HTSLIB):
	./build_htslib_static.sh $(realpath $(HTSLIB_PREFIX))

memcheck tsan: all
nodebug: all
release: all
debug: all

OBJS := GBase.o GStr.o GArgs.o GFaSeqGet.o GFastaIndex.o gff.o gdna.o codons.o

version: ; @echo "GCC Version is: "$(GCC_MAJOR)":"$(GCC_MINOR)":"$(GCC_SUB)
GFastaIndex.o: $(HTSLIB) GFastaIndex.h GFastaIndex.cpp
gtest.o: $(HTSLIB) gtest.cpp GBase.h gff.h GFaSeqGet.h GFastaIndex.o
gtest:  $(HTSLIB) $(OBJS) gtest.o
	$(LINKER) $(LDFLAGS) -o $@ $(filter-out %.a %.so, $^) $(LIBS)
HTSLIB_BUILT := ./.htslib.built
# target for removing all object files
.PHONY : clean
clean:: 
	$(RM) $(OBJS) *.o gtest$(EXE)
	$(RM) core.*
cleanall:: 
	$(RM) -r htslib-* $(OBJS) *.o gtest$(EXE)
	$(RM) core.*
ifneq ("$(wildcard $(HTSLIB_BUILT))","")
	@echo "   HTSLIB was built by our script. Cleaning up..."
	$(RM) $(HTSLIB_BUILT) $(HTSLIB) $(HTSLIB_PREFIX)/bin/bgzip $(HTSLIB_PREFIX)/bin/tabix $(HTSLIB_PREFIX)/bin/htsfile $(HTSLIB_PREFIX)/bin/annot-tsv
	$(RM) -r $(HTSINC)/htslib
endif
