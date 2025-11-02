#/bin/sh
#  args:
#      [-v|--version=SQUIRREL_VERSION]
#      [gcc|clang|mingw]
#      [32|64]
#      [mbs|unicode]
#      [single|double]
#      [gc|nogc]
#      [sqstdlib|nosqstdlib]
#      [ffi|noffi]
#      [static|{shared|dll}]
#      [release|debug]
#      [--std=c++11]
#      [asan]
#      [clean]
#
#  default: gcc 64 mbs single gc sqstdlib noffi static release

pushd()
{
	command pushd "$@" > /dev/null
}

popd()
{
	command popd "$@" > /dev/null
}

WORKDIR=$(pwd)
DIR_SQUIRREL="${WORKDIR}/../squirrel"
DIR_SQDBG="${WORKDIR}/../sqdbg"
DIR_LIBFFI="${WORKDIR}/../libffi"
LIBFFI_VERSION=8

clean=
debug=
x64=1
dll=
mingw=
unicode=
double=
nogc=
nosqstdlib=
ffi=
sqver=
compile_exe="g++"
cppstd="c++11"
lib_exe="ar rcs"
link_exe=
asan=

while [ $# -gt 0 ]; do
	case $1 in
		clean) clean=1 ;;
		debug) debug=1 ;;
		release) debug= ;;
		gcc) compile_exe="g++" ;;
		clang) compile_exe="clang++" ;;
		mingw) mingw=1 ;;
		64) x64=1 ;;
		32) x64= ;;
		unicode) unicode=1 ;;
		mbs) unicode= ;;
		double) double=1 ;;
		single) double= ;;
		gc) nogc= ;;
		nogc) nogc=1 ;;
		sqstdlib) nosqstdlib= ;;
		nosqstdlib) nosqstdlib=1 ;;
		ffi) ffi=1 ;;
		noffi) ffi= ;;
		dll) dll=1 ;;
		shared) dll=1 ;;
		static) dll= ;;
		-v) shift; sqver=$1 ;;
		--version=*) sqver="${1#--version=}" ;;
		--std=*) cppstd="${1#--std=}" ;;
		asan) asan=1 ;;
		*) echo "unknown argument $1"; exit 1 ;;
	esac
	shift
done

echo "work dir: $WORKDIR"

TAG=

if [ ! -z "$sqver" ]; then
	DIR_SQUIRREL=${DIR_SQUIRREL}_${sqver}
	TAG=$sqver
fi

[ ! -d "$DIR_SQUIRREL" ] && return
[ ! -d "bin" ] && mkdir bin
[ ! -d "build" ] && mkdir build

if [ ! -z "$mingw" ]; then
	TAG="${TAG}_mingw32"
	if [ ! -z "$x64" ]; then
		compile_exe="x86_64-w64-mingw32-g++"
		lib_exe="x86_64-w64-mingw32-ar rcs"
	else
		compile_exe="i686-w64-mingw32-g++"
		lib_exe="i686-w64-mingw32-ar rcs"
	fi
elif [ "$compile_exe" = "g++" ]; then
	TAG="${TAG}_gcc"
elif [ "$compile_exe" = "clang++" ]; then
	TAG="${TAG}_clang"
else
	TAG="${TAG}_${compile_exe}"
fi

if [ "$OSTYPE" = "cygwin" ]; then
	lib_exe="gcc-ar rcs"
fi

link_exe=$compile_exe

if [ ! -z "$OSTYPE" ]; then
	TAG="${TAG}_${OSTYPE}"
fi

if [ ! -z "$x64" ]; then
	TAG="${TAG}_64"
else
	TAG="${TAG}_32"
fi

if [ ! -z "$unicode" ]; then
	TAG="${TAG}_unicode"
fi

if [ ! -z "$double" ]; then
	TAG="${TAG}_double"
fi

if [ ! -z "$nogc" ]; then
	TAG="${TAG}_nogc"
fi

if [ ! -z "$debug" ]; then
	TAG="${TAG}_debug"
fi

if [ ! -z "$dll" ]; then
	TAG="${TAG}_dll"
	if [ ! -z "$mingw" ] || [ "$OSTYPE" = "cygwin" ]; then
		libext="dll"
	else
		libext="so"
	fi
else
	libext="a"
fi

DIR_BIN="${WORKDIR}/bin"
SQ_PATH_BIN="${DIR_BIN}/sq${TAG}"
DIR_BUILD="${WORKDIR}/build/sq${TAG}"

HASH_SQ=$(git describe --always --dirty)
pushd "$DIR_SQUIRREL"
if [ -d ".git" ]; then
	HASH_SQUIRREL=$(git describe --always --dirty)
else
	HASH_SQUIRREL=0
fi
popd
pushd "$DIR_SQDBG"
HASH_SQDBG=$(git describe --always --dirty)
popd

compile_flags="-c -g -std=$cppstd -frtti -fno-exceptions -fno-strict-aliasing"
squirrel_flags="-w -I${DIR_SQUIRREL}/include"
link_flags="$link_flags -L${DIR_BUILD}"

if [ "$compile_exe" = "g++" ] || [ ! -z "$mingw" ]; then
	sqdbg_flags="-fvisibility=hidden -fvisibility-inlines-hidden -Wall -Wextra -Wno-class-memacces -Wno-implicit-fallthrough -Wno-int-in-bool-context -Wno-strict-aliasing -Wno-suggest-attribute=const -Wno-suggest-attribute=pure -Wno-type-limits"

	if [ ! -z "$sqver" ] && [ "$sqver" -lt 300 ]; then
		compile_flags="$compile_flags -fpermissive"
	fi
elif [ "$compile_exe" = "clang++" ]; then
	sqdbg_flags="-fvisibility=hidden -fvisibility-inlines-hidden -Wall -Wextra -Wno-int-in-bool-context -Wno-strict-aliasing"
fi

sq_flags="-fvisibility=hidden -Wall -Wextra -Wno-implicit-fallthrough"

if [ ! -z "$mingw" ]; then
	if [ ! -z "$x64" ]; then
		compile_flags="$compile_flags -I/usr/x86_64-w64-mingw32/include"
	else
		compile_flags="$compile_flags -I/usr/i686-w64-mingw32/include"
	fi
fi

sqdbg_flags="$sqdbg_flags -I${DIR_SQDBG}/include -I${DIR_SQUIRREL}/include -I${DIR_SQUIRREL}/squirrel"
sqdbg_flags="$sqdbg_flags -DSQDBG_SOURCENAME_HAS_PATH -DSQDBG_SUPPORTS_FUNCPROTO_LIST -DSQDBG_USE_COMPILER_FOR_REPL -DSQDBG_SUPPORTS_SET_INSTRUCTION -DSQDBG_COMPILER_MAX_PARAMETER_COUNT=20 -DSQDBG_COMPILER_POW_OP"
sq_flags="$sq_flags -I${DIR_SQDBG}/include -I${DIR_SQUIRREL}/include -I${DIR_SQUIRREL}/squirrel"
sq_flags="$sq_flags -DSQ_BUILD_TAG=\"${TAG}\" -DSQ_HASH_SQ=\"${HASH_SQ}\" -DSQ_HASH_SQUIRREL=\"${HASH_SQUIRREL}\" -DSQ_HASH_SQDBG=\"${HASH_SQDBG}\""

if [ ! -z "$mingw" ] || [ "$OSTYPE" = "cygwin" ]; then
	link_libraries="-lkernel32 -luser32"

	if [ -z "$dll" ]; then
		link_libraries="$link_libraries -lws2_32"
	fi

	link_flags="$link_flags -static-libgcc -static-libstdc++"
fi

if [ ! -z "$x64" ]; then
	compile_flags="$compile_flags -m64 -D_SQ64"
	link_flags="$link_flags -m64"
else
	compile_flags="$compile_flags -m32"
	link_flags="$link_flags -m32"
fi

if [ ! -z "$debug" ]; then
	compile_flags="$compile_flags -O0 -D_DEBUG"
	sqdbg_flags="$sqdbg_flags -DSQDBG_VALIDATE_SENT_MSG"
else
	compile_flags="$compile_flags -O2 -flto=auto"
	link_flags="$link_flags -flto=auto"
fi

if [ ! -z "$dll" ]; then
	compile_flags="$compile_flags -fPIC"
	link_flags="$link_flags -Wl,-rpath=${DIR_BUILD}"
	link_dll_flags="$link_flags -shared -fPIC"

	if [ -z "$nosqstdlib" ]; then
		link_libraries="-l:libsqdbg.$libext -l:libsquirrel.$libext -l:libsqstdlib.$libext $link_libraries"
	else
		link_libraries="-l:libsqdbg.$libext -l:libsquirrel.$libext $link_libraries"
	fi

	if [ ! -z "$mingw" ]; then
		squirrel_dll_flags="$squirrel_flags -DSQUIRREL_API=__declspec(dllexport)"
		sqdbg_flags="$sqdbg_flags -DSQDBG_DLL -DSQDBG_DLL_EXPORT -DSQUIRREL_API=extern"
		sq_flags="$sq_flags -DSQDBG_DLL -DSQUIRREL_API=__declspec(dllimport)"
	else
		squirrel_dll_flags="$squirrel_flags -DSQUIRREL_API=__attribute__((visibility(\"default\")))"
		squirrel_flags="$squirrel_flags -DSQUIRREL_API=__attribute__((visibility(\"hidden\")))"
		sqdbg_flags="$sqdbg_flags -DSQDBG_DLL -DSQDBG_DLL_EXPORT -DSQUIRREL_API=__attribute__((visibility(\"hidden\")))"
		sq_flags="$sq_flags -DSQDBG_DLL -DSQUIRREL_API=extern"
	fi
else
	squirrel_dll_flags="$squirrel_flags"
	if [ -z "$nosqstdlib" ]; then
		link_libraries="-l:libsqdbg.a -l:libsquirrel_static.a -l:libsqstdlib.a $link_libraries"
	else
		link_libraries="-l:libsqdbg.a -l:libsquirrel_static.a $link_libraries"
	fi
fi

if [ ! -z "$nosqstdlib" ]; then
	sq_flags="$sq_flags -DNOSQSTDLIB"
fi

if [ ! -z "$unicode" ]; then
	compile_flags="$compile_flags -DSQUNICODE"
fi

if [ ! -z "$double" ]; then
	compile_flags="$compile_flags -DSQUSEDOUBLE"
fi

if [ ! -z "$nogc" ]; then
	compile_flags="$compile_flags -DNO_GARBAGE_COLLECTOR"
fi

if [ ! -z "$sqver" ]; then
	compile_flags="$compile_flags -DSQUIRREL_VERSION_NUMBER=$sqver"
fi

if [ ! -z "$ffi" ]; then
	sq_flags="$sq_flags -DSQFFI"

	if [ ! -z "$mingw" ] || [ "$OSTYPE" = "cygwin" ]; then
		if [ ! -z "$x64" ]; then
			sq_flags="$sq_flags -I${DIR_LIBFFI}/win64/include"
			link_libraries="$link_libraries -L${DIR_LIBFFI}/win64/lib -l:libffi-${LIBFFI_VERSION}.lib"
		else
			sq_flags="$sq_flags -I${DIR_LIBFFI}/win32/include"
			link_libraries="$link_libraries -L${DIR_LIBFFI}/win32/lib -l:libffi-${LIBFFI_VERSION}.lib"
		fi
	else
		link_libraries="$link_libraries -l:libffi.$libext"
	fi
fi

if [ ! -z "$asan" ]; then
	compile_flags="$compile_flags -fsanitize=address -static-libasan"
	link_flags="$link_flags -fsanitize=address"
fi

if [ ! -z "$clean" ]; then
	[ -d "$DIR_BUILD" ] && rm -r "$DIR_BUILD"
	[ -f "$SQ_PATH_BIN" ] && rm "$SQ_PATH_BIN"
fi

[ ! -d "$DIR_BUILD" ] && mkdir "$DIR_BUILD"

pushd "$DIR_BUILD"

echo "============ Build sq${TAG} ============"

squirrel_sources="${DIR_SQUIRREL}/squirrel/sqapi.cpp ${DIR_SQUIRREL}/squirrel/sqbaselib.cpp ${DIR_SQUIRREL}/squirrel/sqclass.cpp ${DIR_SQUIRREL}/squirrel/sqcompiler.cpp ${DIR_SQUIRREL}/squirrel/sqdebug.cpp ${DIR_SQUIRREL}/squirrel/sqfuncstate.cpp ${DIR_SQUIRREL}/squirrel/sqlexer.cpp ${DIR_SQUIRREL}/squirrel/sqmem.cpp ${DIR_SQUIRREL}/squirrel/sqobject.cpp ${DIR_SQUIRREL}/squirrel/sqstate.cpp ${DIR_SQUIRREL}/squirrel/sqtable.cpp ${DIR_SQUIRREL}/squirrel/sqvm.cpp"
squirrel_objs="sqapi.o sqbaselib.o sqclass.o sqcompiler.o sqdebug.o sqfuncstate.o sqlexer.o sqmem.o sqobject.o sqstate.o sqtable.o sqvm.o"

if [ ! -f "libsquirrel_static.a" ]; then
	echo "------ Compile squirrel (static) ($HASH_SQUIRREL)"
	$compile_exe -o "sqpcheader.h.gch" $compile_flags $squirrel_flags "${DIR_SQUIRREL}/squirrel/sqpcheader.h"
	[ $? -ne 0 ] && exit 1

	$compile_exe $compile_flags $squirrel_flags -Winvalid-pch -include "${DIR_BUILD}/sqpcheader.h" $squirrel_sources
	[ $? -ne 0 ] && exit 1

	echo "------ Lib squirrel (static)"
	$lib_exe libsquirrel_static.a $squirrel_objs
	[ $? -ne 0 ] && exit 1
fi

if [ ! -z "$dll" ] && [ ! -f "libsquirrel.$libext" ]; then
	rm $squirrel_objs

	echo "------ Compile squirrel ($HASH_SQUIRREL)"
	$compile_exe -o "sqpcheader.h.gch" $compile_flags $squirrel_dll_flags "${DIR_SQUIRREL}/squirrel/sqpcheader.h"
	[ $? -ne 0 ] && exit 1

	$compile_exe $compile_flags $squirrel_dll_flags -Winvalid-pch -include "${DIR_BUILD}/sqpcheader.h" $squirrel_sources
	[ $? -ne 0 ] && exit 1

	echo "------ Lib squirrel"
	$link_exe -o libsquirrel.$libext $link_dll_flags $squirrel_objs
	[ $? -ne 0 ] && exit 1
fi

if [ -z "$nosqstdlib" ] && [ ! -f "libsqstdlib.$libext" ]; then
	echo "------ Compile sqstdlib ($HASH_SQUIRREL)"
	$compile_exe $compile_flags $squirrel_dll_flags "${DIR_SQUIRREL}/sqstdlib/sqstdaux.cpp" "${DIR_SQUIRREL}/sqstdlib/sqstdblob.cpp" "${DIR_SQUIRREL}/sqstdlib/sqstdio.cpp" "${DIR_SQUIRREL}/sqstdlib/sqstdmath.cpp" "${DIR_SQUIRREL}/sqstdlib/sqstdrex.cpp" "${DIR_SQUIRREL}/sqstdlib/sqstdstream.cpp" "${DIR_SQUIRREL}/sqstdlib/sqstdstring.cpp" "${DIR_SQUIRREL}/sqstdlib/sqstdsystem.cpp"
	[ $? -ne 0 ] && exit 1

	echo "------ Lib sqstdlib"
	if [ ! -z "$dll" ]; then
		$link_exe -o libsqstdlib.$libext $link_dll_flags sqstdaux.o sqstdblob.o sqstdio.o sqstdmath.o sqstdrex.o sqstdstream.o sqstdstring.o sqstdsystem.o -l:libsquirrel.$libext
		[ $? -ne 0 ] && exit 1
	else
		$lib_exe libsqstdlib.a sqstdaux.o sqstdblob.o sqstdio.o sqstdmath.o sqstdrex.o sqstdstream.o sqstdstring.o sqstdsystem.o
		[ $? -ne 0 ] && exit 1
	fi
fi

if [ ! -f "libsqdbg.$libext" ]; then
	echo "------ Compile sqdbg ($HASH_SQDBG)"
	$compile_exe -o sqdbg.o $compile_flags $sqdbg_flags "${DIR_SQDBG}/sqdbg/server.cpp"
	[ $? -ne 0 ] && exit 1

	echo "------ Lib sqdbg"
	if [ ! -z "$dll" ]; then
		sqdbglink=-l:libsquirrel_static.a
		if [ ! -z "$mingw" ] || [ "$OSTYPE" = "cygwin" ]; then
			sqdbglink="$sqdbglink -lws2_32"
		fi
		$link_exe -o libsqdbg.$libext sqdbg.o $link_dll_flags $sqdbglink
		[ $? -ne 0 ] && exit 1
	else
		$lib_exe libsqdbg.a sqdbg.o
		[ $? -ne 0 ] && exit 1
	fi
fi

echo "------ Compile sq${TAG} ($HASH_SQ)"
$compile_exe -o "${DIR_BUILD}/sq.o" $compile_flags $sq_flags "${WORKDIR}/sq.cpp"
[ $? -ne 0 ] && exit 1

echo "------ Link sq${TAG}"
$link_exe -o "$SQ_PATH_BIN" $link_flags sq.o $link_libraries
[ $? -ne 0 ] && exit 1

if [ ! -z "$mingw" ] || [ "$OSTYPE" = "cygwin" ]; then
	if [ ! -z "$dll" ]; then
		cp libsquirrel.dll "${DIR_BIN}/libsquirrel.dll"
		[ -z "$nosqstdlib" ] && cp libsqstdlib.dll "${DIR_BIN}/libsqstdlib.dll"
		cp libsqdbg.dll "${DIR_BIN}/libsqdbg.dll"
	fi
fi

rm *.o
popd

echo "output: $SQ_PATH_BIN"

