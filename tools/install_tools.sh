#!/usr/bin/env bash

if [ $# -eq 1 ];then
    make $1
else
    make
fi

echo ""
echo ""
if [ $# -eq 1 ] && [ $1 == "clean" ];then
    echo -e "\033[32m Clean done!"
else
    echo -e "\033[32m The tools is installed in $(pwd) done!"
fi
echo ""
