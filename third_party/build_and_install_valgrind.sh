#!/bin/bash
# Valgrind sources reside in third_party/valgrind.
# To be safe, we sync to a specific version which is known to work.

VALGRIND_INST_ROOT="$1"
shift # The rest are valgrind's configure args.

if [ "$VALGRIND_INST_ROOT" == "" ]; then
  echo "Usage: $0 /tsan/installation/path"
  exit
fi

cd valgrind

make distclean
./autogen.sh && \
  ./configure --prefix=$VALGRIND_INST_ROOT $* && \
  make -s -j 8 && \
  make -s install
