#!/bin/bash

# verify backward compatibility of API/ABI changes

set -e
set -x

# the parameter is check limit
if [[ -n "$1" ]]; then
	re='^[0-9]+$'
	if [[ $1 =~ $re ]]; then
		lim=$1
	else
		echo "input error: not a number" >&2; exit 1
	fi
else
	lim=2
fi

# exit if not ABI check
if [ "${EVENT_ABI_CHECK:-}" != "1" ]; then
	exit 0
fi

EVENT_SOURCE_DIR=$(cd $(dirname $0)/.. && pwd)
ABI_CHECK_ROOT=${HOME}/abi-check
ABI_CHECK_WORKSPACE="work/abi-check"

## ------------------------------------------ ##
## install required packages:wdiff,rfcdiff,   ##
## universal-ctags,abi-tracker                ##
## ------------------------------------------ ##

mkdir ${ABI_CHECK_ROOT}
cd ${ABI_CHECK_ROOT}
mkdir tools

# install wdiff
wget -qO - http://mirrors.kernel.org/gnu/wdiff/wdiff-latest.tar.gz | tar -xz
cd wdiff-*
./configure --prefix=${ABI_CHECK_ROOT}/tools &>/dev/null
make &>/dev/null
make install &>/dev/null
cd ..
export PATH=${ABI_CHECK_ROOT}/tools/bin:$PATH

# install rfcdiff
wget https://tools.ietf.org/tools/rfcdiff/rfcdiff
chmod +x rfcdiff
mv rfcdiff ${ABI_CHECK_ROOT}/tools/bin/

# install universal-ctags
wget https://github.com/universal-ctags/ctags/archive/master.zip
unzip -q master.zip
cd ctags-master
sh ./autogen.sh &>/dev/null
./configure --prefix=${ABI_CHECK_ROOT}/tools &>/dev/null
make &>/dev/null
make install &>/dev/null
cd ..

# install LVC toolset
wget -qO - https://github.com/lvc/installer/archive/0.16.tar.gz | tar -xz
make -C installer-0.16 install prefix=${ABI_CHECK_ROOT}/tools target=abi-tracker

## ------------------------------------------ ##
## run ABI check scripts                      ##
## ------------------------------------------ ##

mkdir -p ${ABI_CHECK_WORKSPACE}
cd ${ABI_CHECK_WORKSPACE}

# copy current source code and profile into workspace
mkdir -p src/libevent/current
mkdir -p installed/libevent/current
cd ${EVENT_SOURCE_DIR}
cp -r $(ls | grep -v ABI) ${ABI_CHECK_ROOT}/${ABI_CHECK_WORKSPACE}/src/libevent/current/
cp ABI/libevent.json ${ABI_CHECK_ROOT}/${ABI_CHECK_WORKSPACE}/
cd -

# run LVC tools
abi-monitor -get -limit $lim libevent.json
abi-monitor -v current -build libevent.json
abi-monitor -build libevent.json
abi-tracker -build libevent.json

# remove useless files
rm -rf src installed build_logs libevent.json
