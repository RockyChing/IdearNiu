#!/bin/bash

PWD=`pwd`
FILES=`ls`
FILES_NAME=$(echo $FILES|tr " " "\n")

for file in $FILES_NAME; do
	if [ -e $file ]; then
		echo $file "found"
		echo $file|grep -q "tar.bz2"
		if [ $? -eq 0 ]; then
			echo $file "valid"
			tar xvf $file && rm -rf $file
		else
			echo $file "invalid"
		fi
	else
		echo $file "is invalid"
	fi
done

