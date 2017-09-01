#!/bin/bash

ROOT_PATH=`pwd`

#SVN_VERSION=`svn --version | grep "svn, version" | awk '{print $3}'`

VERSION_C="version.c"
VERSION_C_PATH=$ROOT_PATH/$VERSION_C

DEFAULT_FW_REVISION="0000"
FW_REVISION=`svn info | grep "Last Changed Rev" | awk '{print $4}'`
FW_BUILD_DATE=`date +%Y-%m-%d` 
FW_BUILD_HOUR=`date +%H:%M`
#if [ "$SVN_VERSION" != "1.7.9" ]; then
#	printf "Only update version automatically in SVN 1.7.9\n"
#	exit
#fi

#printf "SVN version:$SVN_VERSION\n"
if [ "$FW_REVISION" == "" ]; then
#    printf "WIFI_FW revision:\33[35mnot found\33[0m\n"
    FW_REVISION=$DEFAULT_REVISION
else 
    #printf "WIFI_FW revision:$FW_REVISION\n"
	printf "FW BUILD DATE: $FW_BUILD_DATE $FW_BUILD_HOUR\n"
    sed -i 's/#define SERIAL_NUM ".*"/#define SERIAL_NUM "'$FW_REVISION'"/' $VERSION_C_PATH
    sed -i 's/#define BUILD_DATE ".*"/#define BUILD_DATE "'$FW_BUILD_DATE' '$FW_BUILD_HOUR'"/' $VERSION_C_PATH
fi


