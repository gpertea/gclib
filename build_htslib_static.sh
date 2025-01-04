#!/bin/bash
dlver="1.21"

## this script downloads and builds htslib w/ libdeflate, bz2 and lzma libraries
## to be statically linked
## htslib include files will end up directly in the `htslib` subdirectory
#
## the other dependencies are installed under `htslib/xlibs` prefix
## with the content of `htslib/xlibs/lib/`
## moved to `htslib/` at the end of the build
#
## The build directory is htslib-1.21 which will also have
## lzma, libdeflate and bzip2 build folders
#
## NOTE: both build and install folders will be deleted
## if `clean` parameter is given to this script
err_exit() {
 echo -e "Error: $1" >&2
 exit 1
}

## this must run in the gclib directory

if [[ "$1" == "clean" ]]; then
  /bin/rm -rf htslib*
  /bin/rm -f *.tar.?z*
  exit
fi


cwd=$(pwd -P) ## gclib directory (absolute path
flibdir=$cwd/htslib # final location for library files and htslib headers
prefix=$cwd/htslib/xlibs # local install prefix for dependencies
incdir=$prefix/include
libdir=$prefix/lib

if [[ -f htslib/libhts.a && -f htslib/libdeflate.a && -f htslib/sam.h ]]; then
 echo "htslib already built."
 exit
fi

if [[ -d htslib ]]; then
 mkdir htslib || err_exit "could not create $incdir"
fi
## proceed to build dependencies
if [[ ! -d $incdir ]]; then
  mkdir -p $incdir
  mkdir -p $libdir
fi

## check and download htslib build package
if [[ ! -f htslib-$dlver || ! -d htslib-$dlver/Makefile ]]; then
  dlurl="https://github.com/samtools/htslib/releases/download/$dlver/htslib-$dlver.tar.bz2"
  curl -ksLO "$dlurl" || err_exit "failed to fetch: $dlurl!"
  tar -xjf htslib-$dlver.tar.bz2 || err_exit "failed to unpack htslib-$dlver.tar.bz2"
  unlink htslib-$dlver.tar.bz2
fi

cd htslib-$dlver || err_exit "could not cd htslib-$dlver"

###  from now in
## build and install dependencies first

# -- prepare libdeflate
if [[ ! -f $flibdir/libdeflate.a ]]; then
  if [[ ! -d libdeflate ]]; then
   git clone https://github.com/ebiggers/libdeflate
   cd libdeflate
   #git checkout '9b565afd996d8b798fc7b94cddcc7cfa49293050'
   git checkout 7805198 # release v1.23
   cd ..
  fi
  cd libdeflate
  MINGW=''
  libdeflate=libdeflate.a
  if [[ $(gcc -dumpmachine) == *mingw* ]]; then
   MINGW=1
   libdeflate=libdeflatestatic.lib
  fi
  make -f $cwd/Makefile.libdeflate -j 4 $libdeflate || exit 1
  mv libdeflate.h $incdir/
  mv $libdeflate $libdir/libdeflate.a
  cd ..
fi

bzip="bzip2-1.0.8"
if [[ ! -f $flibdir/libbz2.a ]]; then
  if [[ ! -d bzip2 ]]; then
    curl -ksLO https://sourceware.org/pub/bzip2/$bzip.tar.gz || \
      exec echo "Error: failed to fetch $bzip.tar.gz!"
    tar -xzf $bzip.tar.gz || exec echo "Error: failed to unpack $bzip.tar.gz!"
    /bin/rm -f $bzip.tar.gz
    mv $bzip bzip2
  fi
  cd bzip2
  make -j 4 libbz2.a
  mv bzlib.h $incdir/
  mv libbz2.a $libdir/
  cd ..
fi

# -- prepare liblzma
xzver="5.4.7"
xz="xz-$xzver"
if [[ ! -f $flibdir/liblzma.a ]]; then
  if [[ ! -d lzma ]]; then
    xzurl="https://github.com/tukaani-project/xz/releases/download/v$xzver/$xz.tar.gz"
    curl -ksLO "$xzurl" || exec echo "Error: failed to fetch: $xzurl!"
    tar -xzf $xz.tar.gz || exec echo "Error: failed to unpack $xz.tar.gz !"
    unlink $xz.tar.gz
    mv $xz lzma
  fi
  cd lzma
  ./configure --disable-shared -disable-xz -disable-xzdec --disable-lzmadec \
   --disable-lzmainfo --disable-nls --prefix=$prefix
  make -j 4
  make install
  cd ..
fi

#echo "Now build htslib with libdeflate enabled"

# Check if install-lib-static target exists in the Makefile
if ! grep -q "^install-lib-static:" Makefile; then
  echo "Adding install-lib-static target to the Makefile..."
  perl -i -pe 's/^install:/install-lib-static: lib-static \$(BUILT_PROGRAMS) install-pkgconfig\
\t\$(INSTALL_DIR) \$(DESTDIR)\$(bindir) \$(DESTDIR)\$(includedir) \$(DESTDIR)\$(includedir)\/htslib \$(DESTDIR)\$(libdir)\
\t\$(INSTALL_PROGRAM) \$(BUILT_PROGRAMS) \$(DESTDIR)\$(bindir)\
\t\$(INSTALL_DATA) libhts.a \$(DESTDIR)\$(libdir)\/libhts.a\
\t\$(INSTALL_DATA) \$(SRC)htslib\/\*.h \$(DESTDIR)\$(includedir)\/htslib\
\ninstall:/' Makefile
  #perl -i -pe '$_="" if m/HAVE_LIBCURL 1/' Makefile
  perl -i -pe 's/HAVE_LIBCURL 1/HAVE_LIBDEFLATE 1/' Makefile
  #perl -i -pe 's/ \-lcurl//' Makefile
  perl -i -pe 's/^(htslib_default_libs =).+/$1 -lz -lm -lbz2 -ldeflate -llzma/' Makefile

fi

# Build the static library
echo "Building the static library..."
make -j6 lib-static || err_exit "htslib build failed"

make install-lib-static prefix=$flibdir || err_exit "htslib install failed."
cd $cwd
/bin/rm -rf htslib-$dlver
## move includes and lib in htslib/
mv $flibdir/include/htslib/*.h $flibdir/
mv $flibdir/lib/* $flibdir/
rmdir $flibdir/include/htslib $flibdir/include $flibdir/lib

## move xlibs in ./htslib/ too
mv $libdir/*.a $flibdir/

#touch ../.htslib.built
