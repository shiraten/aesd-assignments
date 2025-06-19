#!/bin/sh

if [ "$#" -ne 2 ]; then
  echo "Usage: finder.sh <path_to_file> <str>"
  exit 1
fi

writefile=${1}
writestr=${2}

if [ ! -d $(dirname "${writefile}") ]; then
    mkdir -p $(dirname "${writefile}")
fi

touch "${writefile}"
if [ "$?" -ne 0 ]; then
    echo "cannot create file ${writefile}"
    exit 1
fi

echo "${writestr}" > "${writefile}"
if [ "$?" -ne 0 ]; then
    echo "cannot write to file ${writefile}"
    exit 1
fi
