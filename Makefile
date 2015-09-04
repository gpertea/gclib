THISCODEDIR := .
SEARCHDIRS := -I${THISCODEDIR}

SYSTYPE :=     $(shell uname)

# A simple hack to check if we are on Windows or not (i.e. are we using mingw32-make?)
ifeq ($(findstring mingw32, $(MAKE)), mingw32)
WINDOWS=1
endif


# Compiler settings
TLIBS = 
LDFLAGS = 
# Non-windows systems need pthread
ifndef WINDOWS
TLIBS += -lpthread
endif

# MinGW32 GCC 4.5 link problem fix
ifdef WINDOWS
ifeq ($(findstring 4.5.,$(shell g++ -dumpversion)), 4.5.)
LDFLAGS += -static-libstdc++ -static-libgcc
endif
endif

# Misc. system commands
ifdef WINDOWS
RM = del /Q
else
RM = rm -f
endif

# File endings
ifdef WINDOWS
EXE = .exe
else
EXE =
endif

CC      := g++

BASEFLAGS  := -Wall -Wextra ${SEARCHDIRS} $(MARCH) \
 -D_REENTRANT -fno-exceptions -fno-rtti

#add the link-time optimization flag if gcc version > 4.5
GCC_VERSION:=$(subst ., ,$(shell gcc -dumpversion))
GCC_MAJOR:=$(word 1,$(GCC_VERSION))
GCC_MINOR:=$(word 2,$(GCC_VERSION))
#GCC_SUB:=$(word 3,$(GCC_VERSION))
GCC_SUB:=x

GCC45OPTS :=
GCC45OPTMAIN :=

ifeq ($(findstring nodebug,$(MAKECMDGOALS)),)
  CFLAGS := -g -DDEBUG $(BASEFLAGS)
  LDFLAGS += -g
else
  CFLAGS := -O2 -DNDEBUG $(BASEFLAGS)
  ifeq ($(shell expr $(GCC_MAJOR).$(GCC_MINOR) '>=' 4.5),1)
    CFLAGS += -flto
    GCC45OPTS := -flto
    GCC45OPTMAIN := -fwhole-program
  endif
endif

%.o : %.cpp
	${CC} ${CFLAGS} -c $< -o $@

# C/C++ linker

LINKER  := g++
LIBS := 
OBJS := GBase.o GStr.o GArgs.o

.PHONY : all
all:    gtest threads

version: ; @echo "GCC Version is: "$(GCC_MAJOR)":"$(GCC_MINOR)":"$(GCC_SUB)
	@echo "> GCC Opt. string is: "$(GCC45OPTS)
debug:  gtest threads
nodebug:  gtest threads
gtest.o : GBase.h GArgs.h GVec.hh GList.hh GBitVec.h
GArgs.o : GArgs.h
gtest: $(OBJS) gtest.o
	${LINKER} ${LDFLAGS} $(GCC45OPTS) $(GCC45OPTMAIN) -o $@ ${filter-out %.a %.so, $^} ${LIBS}
threads: GThreads.o threads.o
	${LINKER} ${LDFLAGS} $(GCC45OPTS) $(GCC45OPTMAIN) -o $@ ${filter-out %.a %.so, $^} ${TLIBS} ${LIBS}
GThreads.o: GThreads.h GThreads.cpp
threads.o : GThreads.h GThreads.cpp
# target for removing all object files

.PHONY : clean
clean:: 
	@${RM} $(OBJS) gtest.o GThreads.o threads.o threads$(EXE) gtest$(EXE)
	@${RM} core.*


