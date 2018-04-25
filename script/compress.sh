#!/bin/bash

PWD=`pwd`
FILES=`ls`
echo "files under "$PWD":"

FILES_NAME=$(echo $FILES|tr " " "\n")

for file in $FILES_NAME; do
	if [ -d $file ]; then
		#echo $file "is a dir"
		if [ $file == "out" ]; then
			echo "exclude out dir"
		else
			# tar zcvf $file $file.tar.bz2
			outdir=$file".tar.bz2"
			echo "compress" $file "to" $outdir
			tar zcvf $outdir $file
		fi
	else
		echo $file "is a file"
	fi
done

