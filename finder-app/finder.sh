#!/bin/sh

if [ "$#" -ne 2 ]; then
  echo "Usage: finder.sh <path_to_dir> <str>"
  exit 1
fi

filesdir=${1}
searchstr=${2}

if [ ! -d "${filesdir}" ]; then
    echo "Error: directory ${filesdir} does not exist"
  exit 1
fi

dir_content=$(find "${filesdir}" -maxdepth 1 -type f)
nb_files=$(find "${filesdir}" -maxdepth 1 -type f | wc -l)

match_cnt=0
for file in $dir_content
do
    match_cnt=$((match_cnt+$(cat "${file}" | grep "${searchstr}" -c)))
done

echo "The number of files are ${nb_files} and the number of matching lines are ${match_cnt}"