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

dir_content=$(find "${filesdir}" -type f)
nb_files=$(find "${filesdir}" -type f | wc -l)

for FILE in $(find "${filesdir}" -type f)
do 
  count=$(cat $FILE | grep "${searchstr}" | wc -l)
  match_cnt=$((match_cnt+$count))
done

echo "The number of files are ${nb_files} and the number of matching lines are ${match_cnt}"