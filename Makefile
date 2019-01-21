INCDIRS := -I. -I${GDIR} -I${BAM}

CXX   := $(if $(CXX),$(CXX),g++)

LINKER  := $(if $(LINKER),$(LINKER),g++)

LDFLAGS := $(if $(LDFLAGS),$(LDFLAGS),-g)

LIBS    := 


# A simple hack to check if we are on Windows or not (i.e. are we using mingw32-make?)
ifeq ($(findstring mingw32, $(MAKE)), mingw32)
WINDOWS=1
endif

# Compiler settings
TLIBS = 
# Non-windows systems need pthread
ifndef WINDOWS
TLIBS += -lpthread
endif



DMACH := $(shell ${CXX} -dumpmachine)

# MinGW32 GCC 4.5 link problem fix
ifdef WINDOWS
DMACH := windows
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

BASEFLAGS  := -Wall -Wextra ${INCDIRS} $(MARCH) \
 -D_REENTRANT -fno-exceptions -fno-rtti

GCCVER5 := $(shell expr `g++ -dumpversion | cut -f1 -d.` \>= 5)
ifeq "$(GCCVER5)" "1"
 BASEFLAGS += -Wno-implicit-fallthrough
endif

GCCVER8 := $(shell expr `g++ -dumpversion | cut -f1 -d.` \>= 8)
ifeq "$(GCCVER8)" "1"
  BASEFLAGS += -Wno-class-memaccess
endif

#add the link-time optimization flag if gcc version > 4.5
GCC_VERSION:=$(subst ., ,$(shell gcc -dumpversion))
GCC_MAJOR:=$(word 1,$(GCC_VERSION))
GCC_MINOR:=$(word 2,$(GCC_VERSION))
#GCC_SUB:=$(word 3,$(GCC_VERSION))
GCC_SUB:=x

GCC45OPTS :=
GCC45OPTMAIN :=

ifneq (,$(filter %release %nodebug, $(MAKECMDGOALS)))
  # -- release build
  CXXFLAGS := $(if $(CXXFLAGS),$(CXXFLAGS),-g -O3)
  CXXFLAGS += -DNDEBUG $(BASEFLAGS)
  ifeq ($(shell expr $(GCC_MAJOR).$(GCC_MINOR) '>=' 4.5),1)
    CXXFLAGS += -flto
    GCC45OPTS := -flto
    GCC45OPTMAIN := -fwhole-program
  endif
else
  # -- debug build
  CXXFLAGS := $(if $(CXXFLAGS),$(CXXFLAGS),-g -O0)
  ifneq (, $(findstring darwin, $(DMACH)))
      CXXFLAGS += -gdwarf-3
  endif
  CXXFLAGS += -DDEBUG -D_DEBUG -DGDEBUG $(BASEFLAGS)
endif

%.o : %.cpp
	${CXX} ${CXXFLAGS} -c $< -o $@


.PHONY : all
all:    mdtest
nodebug: all
release: all
debug: all

OBJS := GBase.o GStr.o GArgs.o

version: ; @echo "GCC Version is: "$(GCC_MAJOR)":"$(GCC_MINOR)":"$(GCC_SUB)
	@echo "> GCC Opt. string is: "$(GCC45OPTS)
mdtest: $(OBJS) mdtest.o
	${LINKER} ${LDFLAGS} $(GCC45OPTS) $(GCC45OPTMAIN) -o $@ ${filter-out %.a %.so, $^} ${LIBS}
# target for removing all object files

.PHONY : clean
clean:: 
	@${RM} $(OBJS) *.o mdtest$(EXE)
	@${RM} core.*
