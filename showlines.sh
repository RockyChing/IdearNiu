#!/bin/bash

SOURCE_FILES=`/usr/bin/find . -name "*.c"|/usr/bin/xargs /usr/bin/wc -l`
HEADER_FILES=`/usr/bin/find . -name "*.h"|/usr/bin/xargs /usr/bin/wc -l`

SOURCE_ARRAY=($SOURCE_FILES)
HEADER_ARRAY=($HEADER_FILES)

function split()
{
    OLD_IFS="$IFS"
	IFS=" "
	#array=$1
	IFS="$OLD_IFS"

	array_len=${#array[@]}
	echo ${array[array_len-2]}
	#for i in ${array[@]};do echo $i ;done
}

array=($SOURCE_FILES)
SOURCE_LINES=$(split)

array=($HEADER_FILES)
HEADER_LINES=$(split)

PROJECT_LINES=`expr $SOURCE_LINES + $HEADER_LINES`
echo $PROJECT_LINES

