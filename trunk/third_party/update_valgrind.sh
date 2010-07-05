#!/bin/bash

update_subversion() {
  svn up
}

checkout() {
  echo "No directory 'valgrind'; doing svn checkout"
  svn co http://valgrind-variant.googlecode.com/svn/trunk/valgrind valgrind
}

if [[ -d valgrind ]]; then
  cd valgrind && update_subversion
else
  checkout && cd valgrind
fi
