#!/bin/sh

fname=~/muhrec3_`uname -s`_`uname -m`_`date +%Y%m%d`_rev`svn info |grep Revision | sed -e 's/Revision: //'`.tar.bz2

echo $fname
pushd .
cd ~/home/
#tar -jcvhf $fname muhrec3
tar -jcvf $fname muhrec3_14
popd
 
