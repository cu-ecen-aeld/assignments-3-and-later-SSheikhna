#!/bin/bash

filesdir=$1
searchstr=$2

if [[ $# -eq 0 ]] ; then 
    echo "no args"
    exit 1
elif [[ ! -d "$filesdir"  ]] ; then
    echo $filesdir "is not dir"
    exit 1
fi
X=0
Y=0
dir_items=$( find $filesdir )
for var in $dir_items
do
    if [[ -f "$var"  ]] ; then
        if [ ! -z $searchstr  ] ; then
            matching_number=$( grep -c $searchstr $var )
            let Y+=$matching_number
        fi
        let X++
    fi
done
echo  "The number of files are $X and the number of matching lines are $Y"
