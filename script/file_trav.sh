#!/bin/bash
# Traversal all files under a specified folder


function file_trav(){ 
	flist=`ls $1`
	cd $1

	for f in $flist
		do
			if [ -d $f ]; then
				file_trav $1/$f
			else
				echo "$1/$f"
			fi
		done
	cd ../ 
}

FOLDER=$1
if [ $# != 1 ]; then
	FOLDER=`dirname $0`
fi

echo "FOLDER=$FOLDER"
file_trav $FOLDER
