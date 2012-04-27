#!/bin/sh

A2ETOOLS_OS="unknown"
A2ETOOLS_PLATFORM="x32"
A2ETOOLS_MAKE="make"
A2ETOOLS_MAKE_PLATFORM="32"
A2ETOOLS_ARGS=""
A2ETOOLS_CPU_COUNT=1
A2ETOOLS_USE_CLANG=0

if [[ $# > 0 && $1 == "gcc" ]]; then
	A2ETOOLS_ARGS="--gcc"
else
	A2ETOOLS_ARGS="--clang"
	A2ETOOLS_USE_CLANG=1
fi

case $( uname | tr [:upper:] [:lower:] ) in
	"darwin")
		A2ETOOLS_OS="macosx"
		A2ETOOLS_CPU_COUNT=$(sysctl -a | grep 'machdep.cpu.thread_count' | sed -E 's/.*(: )([:digit:]*)/\2/g')
		;;
	"linux")
		A2ETOOLS_OS="linux"
		A2ETOOLS_CPU_COUNT=$(cat /proc/cpuinfo | grep -m 1 'cpu cores' | sed -E 's/.*(: )([:digit:]*)/\2/g')
		;;
	[a-z0-9]*"BSD")
		A2ETOOLS_OS="bsd"
		A2ETOOLS_MAKE="gmake"
		# TODO: get cpu/thread count on *bsd
		;;
	"cygwin"*)
		A2ETOOLS_OS="windows"
		A2ETOOLS_ARGS+=" --env cygwin"
		A2ETOOLS_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		;;
	"mingw"*)
		A2ETOOLS_OS="windows"
		A2ETOOLS_ARGS+=" --env mingw"
		A2ETOOLS_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		;;
	*)
		echo "unknown operating system - exiting"
		exit
		;;
esac


A2ETOOLS_PLATFORM_TEST_STRING=""
if [[ $A2ETOOLS_OS != "windows" ]]; then
	A2ETOOLS_PLATFORM_TEST_STRING=$( uname -m )
else
	A2ETOOLS_PLATFORM_TEST_STRING=$( gcc -dumpmachine | sed "s/-.*//" )
fi

case $A2ETOOLS_PLATFORM_TEST_STRING in
	"i386"|"i486"|"i586"|"i686")
		A2ETOOLS_PLATFORM="x32"
		A2ETOOLS_MAKE_PLATFORM="32"
		A2ETOOLS_ARGS+=" --platform x32"
		;;
	"x86_64"|"amd64")
		A2ETOOLS_PLATFORM="x64"
		A2ETOOLS_MAKE_PLATFORM="64"
		A2ETOOLS_ARGS+=" --platform x64"
		;;
	*)
		echo "unknown architecture - using "${A2ETOOLS_PLATFORM}
		exit;;
esac


echo "using: premake4 --cc=gcc --os="${A2ETOOLS_OS}" gmake "${A2ETOOLS_ARGS}

premake4 --cc=gcc --os=${A2ETOOLS_OS} gmake ${A2ETOOLS_ARGS}
sed -i -e 's/\${MAKE}/\${MAKE} -j '${A2ETOOLS_CPU_COUNT}'/' Makefile

if [[ $A2ETOOLS_USE_CLANG == 1 ]]; then
	sed -i '1i export CC=clang' Makefile
	sed -i '1i export CXX=clang++' Makefile
fi

echo ""
echo "###################################################"
echo "# NOTE: use '"${A2ETOOLS_MAKE}"' to build a2etools"
echo "###################################################"
echo ""
