#!/bin/bash

##########################################
#  this file contains:
#   SYMLINK TEST: test for symlink correctness
###########################################

DIR=$1
ORIG_FILE=/etc/hosts

echo "SYMLINK TEST"
rm ${DIR}/hosts ${DIR}/testhostslink ${DIR}/hosts_copy ${DIR}/hosts_copy2 >/dev/null 2>&1

ln -s ${ORIG_FILE} ${DIR}/hosts
diff ${ORIG_FILE} yfs1/hosts >/dev/null 2>&1
if [ $? -ne 0 ];
then
    echo "failed SYMLINK test"
    exit
# else
#     echo "pass d-1"
fi

cp ${ORIG_FILE} ${DIR}/hosts_copy #copy yfs1/hosts to /yfs1/hostscopy
ln -s hosts_copy ${DIR}/testhostslink
diff ${DIR}/testhostslink ${DIR}/hosts_copy >/dev/null 2>&1
if [ $? -ne 0 ];
then
    echo "failed SYMLINK test"
    exit
# else
#     echo "pass d-2"
fi

rm ${DIR}/hosts_copy
touch ${DIR}/hosts_copy >/dev/null 2>&1
diff ${DIR}/testhostslink ${DIR}/hosts_copy >/dev/null 2>&1
if [ $? -ne 0 ];
then 
    echo "failed SYMLINK test"
    exit
# else
#     echo "pass d-3"
fi

echo "Passed SYMLINK TEST"
