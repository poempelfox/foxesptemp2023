#!/bin/bash

if [ ! -e "version.txt" ] ; then
	echo "no version.txt in current dir, nothing to update."
	exit 0
fi
ORIGVER=$(<version.txt)
MAINVER="${ORIGVER%+*}"
BUILDVER="${ORIGVER#*+}"
if [ "${#MAINVER}" -lt "3" ] ; then
	echo "version parse error - invalid MAINVER"
	exit 0
fi
if [ "${#BUILDVER}" -lt "1" ] ; then
	echo "version parse error - invalid BUILDVER"
	exit 0
fi
#echo "orig: ${ORIGVER} main: ${MAINVER} build: ${BUILDVER}"
let "BUILDVER=BUILDVER+1"
#echo "new: ${MAINVER}+${BUILDVER}"
echo "${MAINVER}+${BUILDVER}" > version.txt

