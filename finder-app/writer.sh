#!/bin/bash

writefile=$1
writestr=$2

if [[ ! $# -eq 2 ]] ; then 
    echo " 2 args required"
    exit 1
fi
_dirname=$( dirname $writefile )
mkdir -p $_dirname
touch $writefile
echo $writestr > $writefile
#exit 0
