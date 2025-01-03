#!/bin/bash
dlver="1.21"
install_prefix=""

err_exit() {
 echo -e "Error: $1" >&2
 exit 1
}

if [[ ! -d htslib-$dlver || ! -d htslib-$dlver/Makefile ]]; then
  dlurl="https://github.com/samtools/htslib/releases/download/$dlver/htslib-$dlver.tar.bz2"
  curl -ksLO "$dlurl" || err_exit "failed to fetch: $dlurl!"
  tar -xjf htslib-$dlver.tar.bz2 || err_exit "failed to unpack htslib-$dlver.tar.bz2"
  unlink htslib-$dlver.tar.bz2
fi

cd htslib-$dlver || err_exit "could not cd htslib-$dlver"

## must be in htslib source directory
# Ensure Makefile and sam.c exist in the current directory
if [[ ! -f Makefile || ! -f sam.c ]]; then
  err_exit "this script must be run in the htslib source directory."
fi

# Parse optional install_prefix argument
if [ "$#" -ge 1 ]; then
  install_prefix=$1
fi

# Check if install-lib-static target exists in the Makefile
if ! grep -q "^install-lib-static:" Makefile; then
  echo "Adding install-lib-static target to the Makefile..."
  perl -i -pe 's/^install:/install-lib-static: lib-static \$(BUILT_PROGRAMS) install-pkgconfig\
\t\$(INSTALL_DIR) \$(DESTDIR)\$(bindir) \$(DESTDIR)\$(includedir) \$(DESTDIR)\$(includedir)\/htslib \$(DESTDIR)\$(libdir)\
\t\$(INSTALL_PROGRAM) \$(BUILT_PROGRAMS) \$(DESTDIR)\$(bindir)\
\t\$(INSTALL_DATA) libhts.a \$(DESTDIR)\$(libdir)\/libhts.a\
\t\$(INSTALL_DATA) \$(SRC)htslib\/\*.h \$(DESTDIR)\$(includedir)\/htslib\
\ninstall:/' Makefile
  perl -i -pe '$_="" if m/HAVE_LIBCURL 1/' Makefile
  perl -i -pe 's/ \-lcurl//' Makefile
fi

# Build the static library
echo "Building the static library..."
make -j6 lib-static

# Install the static library and headers if install_prefix is provided
if [ -n "$install_prefix" ]; then
  echo "Installing static library and headers to prefix: $install_prefix..."
  make install-lib-static prefix="$install_prefix" || err_exit " Build failed."
else
  echo "Installation prefix not provided. Build complete."
fi

touch ../.htslib.built
