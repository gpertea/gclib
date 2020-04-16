## BAM  := ./samtools-0.1.18
## GDIR := ./gclib
## INCDIRS := -I. -I${GDIR} -I${BAM}
INCDIRS := -I. 

CXX   := $(if $(CXX),$(CXX),g++)

LINKER  := $(if $(LINKER),$(LINKER),g++)

## LDFLAGS += -L${BAM}
LDFLAGS := $(if $(LDFLAGS),$(LDFLAGS),-g)

## LIBS    := -lbam -lz
LIBS    := 

## ifneq (,$(findstring nothreads,$(MAKECMDGOALS)))
##   NOTHREADS=1
## endif

ifeq ($(OS),Windows_NT)
  WINDOWS=1
  WIN64=
  WIN32=
  ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
    WIN64=1
  else
    ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
      WIN64=1
    endif 
  endif
  ifneq ($(WIN64), 1)
    $(error WIN64 architecture not detected for this Windows system)
  endif
endif

DMACH := $(shell ${CXX} -dumpmachine)

ifdef WIN64
  #only MinGW64 compiler is supported
  ifneq ($(DMACH), x86_64-w64-mingw32)
     #attempt to override CXX accordingly
     CXX=x86_64-w64-mingw32-g++
     ifeq (, $(shell which $(CXX)))
       $(error "Error: compiler $(CXX) not found in PATH !")
     endif
  endif
  #ifeq ($(findstring 4.5.,$(shell g++ -dumpversion)), 4.5.)
  #  LDFLAGS += -static-libstdc++ -static-libgcc
  #endif
  DMACH := windows
  # needed on Windows for GetProcessMemoryInfo etc.:
  LIBS += -lpsapi
endif

# Misc. system commands
ifdef WINDOWS
 #RM = del /Q
 RM = rm -f
 EXE = .exe
else
 #non-Windows systems
 RM = rm -f
 EXE = 
 ### non-Windows systems need pthread
 ##ifndef NOTHREADS
 ##  LIBS := -pthread ${LIBS}
 ##  BASEFLAGS += -pthread
 ##endif
endif

#BASEFLAGS  := -Wall -Wextra ${INCDIRS} $(MARCH) \
# -D_REENTRANT -fno-exceptions -fno-rtti

BASEFLAGS := -Wall -Wextra ${INCDIRS} -fsigned-char -D_FILE_OFFSET_BITS=64 \
 -D_LARGEFILE_SOURCE -fno-strict-aliasing -fno-exceptions -fno-rtti -std=c++0x

GCC_VERSION:=$(subst ., ,$(shell gcc -dumpversion))
GCC_MAJOR:=$(word 1,$(GCC_VERSION))
GCC_MINOR:=$(word 2,$(GCC_VERSION))

GCCVER5 := $(shell expr $(GCC_MAJOR) \>= 5)
ifeq "$(GCCVER5)" "1"
 BASEFLAGS += -Wno-implicit-fallthrough
endif

GCCVER8 := $(shell expr $(GCC_MAJOR) \>= 8)
ifeq "$(GCCVER8)" "1"
  BASEFLAGS += -Wno-class-memaccess
endif

LNKTO :=
LNKMAIN :=

ifeq ($(shell expr $(GCC_MAJOR).$(GCC_MINOR) '<=' 4.5),1)
 $(error "Sorry, gcc version 4.5 or greater expected. ($(GCC_MAJOR).$(GCC_MINOR) found)")
endif

ifneq (,$(filter %env %ver %release %nodebug %static, $(MAKECMDGOALS)))
  # -- release build
  # -- release build
  RELEASE_BUILD=1
  CXXFLAGS := $(if $(CXXFLAGS),$(CXXFLAGS),-g -O3)
  CXXFLAGS += -DNDEBUG $(BASEFLAGS) -flto
  LNKMAIN := -flto -fwhole-program
else
  # -- debug build
  DEBUG_BUILD=1
  CXXFLAGS := $(if $(CXXFLAGS),$(CXXFLAGS),-g -O0)
  ifneq (, $(findstring darwin, $(DMACH)))
      CXXFLAGS += -gdwarf-3
  endif
  CXXFLAGS += -DDEBUG -D_DEBUG -DGDEBUG $(BASEFLAGS)
endif

%.o : %.cpp
	${CXX} ${CXXFLAGS} -c $< -o $@

.PHONY : all
all:    htest
#all:     rusage
nodebug: all
release: all
debug: all

OBJS := GBase.o GStr.o GArgs.o GResUsage.o
htest:  $(OBJS) htest.o GHash.hh
	${LINKER} ${LDFLAGS} $(GCC45OPTS) $(GCC45OPTMAIN) -o $@ ${filter-out %.a %.so, $^} ${LIBS}
ver : env
env: ; @echo "GCC Version is: "$(GCC_MAJOR)":"$(GCC_MINOR)":"$(GCC_SUB)
	@echo "BASEFLAGS = "$(BASEFLAGS)
	@echo "WINDOWS = "$(WINDOWS)" | WIN64 = "$(WIN64)

rusage: $(OBJS) rusage.o
	${LINKER} ${LDFLAGS} $(LNKMAIN) -o $@ ${filter-out %.a %.so, $^} ${LIBS}
mdtest: $(OBJS) mdtest.o
	${LINKER} ${LDFLAGS} $(LNKMAIN) -o $@ ${filter-out %.a %.so, $^} ${LIBS}
# target for removing all object files

.PHONY : clean
clean:: 
	@${RM} $(OBJS) *.o mdtest$(EXE) rusage$(EXE) htest$(EXE)
	@${RM} core.*
