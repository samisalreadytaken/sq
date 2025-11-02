@REM  args:
@REM      [-v|--version=SQUIRREL_VERSION]
@REM      [msvc|clang]
@REM      [32|64]
@REM      [mbs|unicode]
@REM      [single|double]
@REM      [gc|nogc]
@REM      [sqstdlib|nosqstdlib]
@REM      [ffi|noffi]
@REM      [static|{shared|dll}]
@REM      [release|debug]
@REM      [--std=c++14]
@REM      [asan]
@REM      [clean]
@REM      [asm]
@REM
@REM  default: msvc 64 mbs single gc sqstdlib noffi static release
@REM
@REM  SET ASAN_WIN_CONTINUE_ON_INTERCEPTION_FAILURE=1

@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION

SET "WORKDIR=%~dp0"
SET "WORKDIR=%WORKDIR:~0,-1%"
SET "DIR_SQUIRREL=%WORKDIR%\..\squirrel"
SET "DIR_SQDBG=%WORKDIR%\..\sqdbg"
SET "DIR_LIBFFI=%WORKDIR%\..\libffi"
SET LIBFFI_VERSION=8

SET _CLEAN=
SET _DEBUG=
SET _X64=1
SET _DLL=
SET _UNICODE=
SET _DOUBLE=
SET _NOGC=
SET _NOSQSTDLIB=
SET _FFI=
SET SQVER=
SET CPPSTD=c++14
SET _ASM=
SET _COMPILE_EXE=CL
SET _LIB_EXE=LIB.EXE
SET _LINK_EXE=LINK.EXE

IF "%VSCMD_ARG_TGT_ARCH%"=="x64" (
	SET _X64=1
) ELSE IF "%VSCMD_ARG_TGT_ARCH%"=="x86" (
	SET _X64=
)

:ARGS_LOOP
IF "%1"=="clean" (
	SET _CLEAN=1
) ELSE IF "%1"=="debug" (
	SET _DEBUG=1
) ELSE IF "%1"=="release" (
	SET _DEBUG=
) ELSE IF "%1"=="msvc" (
	SET _COMPILE_EXE=CL
) ELSE IF "%1"=="clang" (
	SET _COMPILE_EXE=CLANG-CL
	SET _LINK_EXE=LLD-LINK
) ELSE IF "%1"=="64" (
	SET _X64=1
) ELSE IF "%1"=="32" (
	SET _X64=
) ELSE IF "%1"=="86" (
	SET _X64=
) ELSE IF "%1"=="unicode" (
	SET _UNICODE=1
) ELSE IF "%1"=="mbs" (
	SET _UNICODE=
) ELSE IF "%1"=="double" (
	SET _DOUBLE=1
) ELSE IF "%1"=="single" (
	SET _DOUBLE=
) ELSE IF "%1"=="gc" (
	SET _NOGC=
) ELSE IF "%1"=="nogc" (
	SET _NOGC=1
) ELSE IF "%1"=="sqstdlib" (
	SET _NOSQSTDLIB=
) ELSE IF "%1"=="nosqstdlib" (
	SET _NOSQSTDLIB=1
) ELSE IF "%1"=="ffi" (
	SET _FFI=1
) ELSE IF "%1"=="noffi" (
	SET _FFI=
) ELSE IF "%1"=="dll" (
	SET _DLL=1
) ELSE IF "%1"=="shared" (
	SET _DLL=1
) ELSE IF "%1"=="static" (
	SET _DLL=
) ELSE IF "%1"=="asm" (
	SET _ASM=1
) ELSE IF "%1"=="-v" (
	SET SQVER=%2
	SHIFT
) ELSE IF "%1"=="--version" (
	SET SQVER=%2
	SHIFT
) ELSE IF "%1"=="--std" (
	SET CPPSTD=%2
	SHIFT
) ELSE IF "%1"=="asan" (
	SET _ASAN=1
) ELSE IF "%1"=="autoexit" (
	SET _AUTOEXIT=1
) ELSE IF NOT "%1"=="" (
	ECHO unknown argument %1
	GOTO :EX1
)

SHIFT
IF NOT "%1"=="" GOTO :ARGS_LOOP

ECHO work dir: %WORKDIR%

SET TAG=

IF NOT "!SQVER!"=="" (
	SET DIR_SQUIRREL=!DIR_SQUIRREL!_!SQVER!
	SET TAG=!SQVER!
)

IF NOT EXIST "!DIR_SQUIRREL!" GOTO :EOF
IF NOT EXIST "bin" MKDIR bin
IF NOT EXIST "build" MKDIR build

IF "!_COMPILE_EXE!"=="CL" (
	SET TAG=!TAG!_msvc
) ELSE IF "!_COMPILE_EXE!"=="CLANG-CL" (
	SET TAG=!TAG!_clang-cl
)
IF "!_X64!"=="1" (SET TAG=!TAG!_64) ELSE (SET TAG=!TAG!_86)
IF "!_UNICODE!"=="1" SET TAG=!TAG!_unicode
IF "!_DOUBLE!"=="1" SET TAG=!TAG!_double
IF "!_NOGC!"=="1" SET TAG=!TAG!_nogc
IF "!_DEBUG!"=="1" SET TAG=!TAG!_debug
IF "!_DLL!"=="1" SET TAG=!TAG!_dll

SET "DIR_BIN=%WORKDIR%\bin"
SET "SQ_PATH_BIN=%DIR_BIN%\sq!TAG!"
SET "DIR_BUILD=%WORKDIR%\build\sq!TAG!"

FOR /F %%i IN ('CALL git describe --always --dirty') DO SET "HASH_SQ=%%i"
pushd "%DIR_SQUIRREL%"
IF EXIST ".git" (
	FOR /F %%i IN ('CALL git describe --always --dirty') DO SET "HASH_SQUIRREL=%%i"
) ELSE (
	SET HASH_SQUIRREL=0
)
popd
pushd "%DIR_SQDBG%"
FOR /F %%i IN ('CALL git describe --always --dirty') DO SET "HASH_SQDBG=%%i"
popd

SET LIB_FLAGS=/NOLOGO /NODEFAULTLIB
SET LIB_OUT=/OUT:
SET LINK_FLAGS=/NOLOGO /SUBSYSTEM:CONSOLE /INCREMENTAL:NO /MANIFEST:EMBED /DEBUG
SET LINK_OUT=/OUT:

SET COMPILE_FLAGS=/nologo /c /diagnostics:column /std:!CPPSTD! /Zc:forScope /Zc:wchar_t /Zc:inline /Zc:strictStrings- /fp:precise /external:anglebrackets /external:W0 /TP /MP /EHsc /GF /GS /GR /Zi /Zo /FS
SET COMPILE_OUT=/Fo
SET PDB_OUT=/Fd

SET SQUIRREL_FLAGS=/W0 -I%DIR_SQUIRREL%\include
SET SQDBG_FLAGS=-I%DIR_SQDBG%\include -I%DIR_SQUIRREL%\include -I%DIR_SQUIRREL%\squirrel /Wall /W4 /WX- /wd4061 /wd4127 /wd4146 /wd4244 /wd4267 /wd4365 /wd4582 /wd4668 /wd4710 /wd4800 /wd4820 /wd5039 /wd5045 /wd5262 /wd6297 /wd6320
SET SQ_FLAGS=-I%DIR_SQDBG%\include -I%DIR_SQUIRREL%\include -I%DIR_SQUIRREL%\squirrel /Wall /W4 /WX- /wd4061 /wd4125 /wd4127 /wd4242 /wd4244 /wd4267 /wd4365 /wd4388 /wd4389 /wd4464 /wd4710 /wd4774 /wd4820 /wd5039 /wd5045 /wd5262 /wd6320
SET SQ_FLAGS=!SQ_FLAGS! -DSQ_BUILD_TAG=\"!TAG!\" -DSQ_HASH_SQ=\"!HASH_SQ!\" -DSQ_HASH_SQUIRREL=\"!HASH_SQUIRREL!\" -DSQ_HASH_SQDBG=\"!HASH_SQDBG!\"

IF "!SQVER!" GEQ 300 (
	SET COMPILE_FLAGS=%COMPILE_FLAGS% /permissive-
) ELSE (
	SET COMPILE_FLAGS=%COMPILE_FLAGS% /permissive
)

SET LINK_LIBRARIES=kernel32.lib user32.lib

IF "!_COMPILE_EXE!"=="CLANG-CL" (
	SET COMPILE_FLAGS=!COMPILE_FLAGS! -fno-strict-aliasing -Wno-unused-command-line-argument
	SET SQUIRREL_FLAGS=!SQUIRREL_FLAGS! -w
	SET SQDBG_FLAGS=!SQDBG_FLAGS! -Wall -Wextra -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-cast-align -Wno-cast-function-type-strict -Wno-cast-qual -Wno-covered-switch-default -Wno-double-promotion -Wno-extra-semi -Wno-extra-semi-stmt -Wno-float-conversion -Wno-float-equal -Wno-format-nonliteral -Wno-global-constructors -Wno-gnu-anonymous-struct -Wno-implicit-fallthrough -Wno-implicit-int-conversion -Wno-int-in-bool-context -Wno-invalid-offsetof -Wno-language-extension-token -Wno-missing-prototypes -Wno-missing-variable-declarations -Wno-nested-anon-types -Wno-old-style-cast -Wno-reserved-identifier -Wno-reserved-macro-identifier -Wno-shadow -Wno-shadow-field -Wno-shadow-field-in-constructor -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-strict-aliasing -Wno-string-conversion -Wno-string-plus-int -Wno-suggest-destructor-override -Wno-suggest-override -Wno-switch-enum -Wno-unsafe-buffer-usage -Wno-unused-macros -Wno-zero-as-null-pointer-constant
	SET SQ_FLAGS=!SQ_FLAGS! -Wall -Wextra -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-cast-align -Wno-cast-function-type-strict -Wno-cast-qual -Wno-date-time -Wno-extra-semi -Wno-extra-semi-stmt -Wno-format-nonliteral -Wno-implicit-fallthrough -Wno-implicit-int-conversion -Wno-language-extension-token -Wno-missing-prototypes -Wno-missing-variable-declarations -Wno-old-style-cast -Wno-reserved-identifier -Wno-reserved-macro-identifier -Wno-reserved-macro-identifier -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-string-conversion -Wno-suggest-destructor-override -Wno-suggest-override -Wno-switch-enum -Wno-unsafe-buffer-usage -Wno-unused-macros -Wno-zero-as-null-pointer-constant
)

SET COMPILE_FLAGS=!COMPILE_FLAGS! -D_CRT_SECURE_NO_WARNINGS
SET SQDBG_FLAGS=!SQDBG_FLAGS! -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS -DSQDBG_DEBUGGER_ECHO_OUTPUT -DSQDBG_SOURCENAME_HAS_PATH -DSQDBG_SUPPORTS_FUNCPROTO_LIST -DSQDBG_USE_COMPILER_FOR_REPL -DSQDBG_SUPPORTS_SET_INSTRUCTION -DSQDBG_COMPILER_MAX_PARAMETER_COUNT=20 -DSQDBG_COMPILER_POW_OP
REM SET SQ_FLAGS=!SQ_FLAGS! -DSQDBG_NATIVE_STACKTRACE

IF "!_X64!"=="1" (
	SET LIB_FLAGS=!LIB_FLAGS! /MACHINE:X64
	SET LINK_FLAGS=!LINK_FLAGS! /MACHINE:X64
	SET COMPILE_FLAGS=!COMPILE_FLAGS! -D_SQ64
) ELSE (
	SET LIB_FLAGS=!LIB_FLAGS! /MACHINE:X86
	SET LINK_FLAGS=!LINK_FLAGS! /MACHINE:X86
)

IF "!_DEBUG!"=="1" (
	SET COMPILE_FLAGS=!COMPILE_FLAGS! /Od /Ob1 /RTC1 -D_DEBUG -D_CRTDBG_MAP_ALLOC
	SET SQDBG_FLAGS=!SQDBG_FLAGS! -DSQDBG_VALIDATE_SENT_MSG
) ELSE (
	SET COMPILE_FLAGS=!COMPILE_FLAGS! /O2 /Ob1 /GL
	SET LINK_FLAGS=!LINK_FLAGS! /OPT:ICF /LTCG:STATUS
)

IF "!_ASAN!"=="1" (
	SET COMPILE_FLAGS=!COMPILE_FLAGS! /fsanitize=address

	IF NOT "!_COMPILE_EXE!"=="CLANG-CL" (
		SET LINK_FLAGS=!LINK_FLAGS! /INFERASANLIBS
	)

	IF "!_DEBUG!"=="1" (
		IF "%_X64%"=="1" (
			SET LINK_LIBRARIES=!LINK_LIBRARIES! /WHOLEARCHIVE:clang_rt.asan_dbg_dynamic-x86_64.lib
		) ELSE (
			SET LINK_LIBRARIES=!LINK_LIBRARIES! /WHOLEARCHIVE:clang_rt.asan_dbg_dynamic-i386.lib
		)
	) ELSE (
		IF "%_X64%"=="1" (
			SET LINK_LIBRARIES=!LINK_LIBRARIES! /WHOLEARCHIVE:clang_rt.asan_dynamic-x86_64.lib
		) ELSE (
			SET LINK_LIBRARIES=!LINK_LIBRARIES! /WHOLEARCHIVE:clang_rt.asan_dynamic-i386.lib
		)
	)

	IF "!_DLL!"=="1" (
		IF "%_X64%"=="1" (
			SET LINK_LIBRARIES=!LINK_LIBRARIES! /WHOLEARCHIVE:clang_rt.asan_dynamic_runtime_thunk-x86_64.lib
		) ELSE (
			SET LINK_LIBRARIES=!LINK_LIBRARIES! /WHOLEARCHIVE:clang_rt.asan_dynamic_runtime_thunk-i386.lib
		)
	) ELSE (
		IF "%_X64%"=="1" (
			SET LINK_LIBRARIES=!LINK_LIBRARIES! /WHOLEARCHIVE:clang_rt.asan_static_runtime_thunk-x86_64.lib
		) ELSE (
			SET LINK_LIBRARIES=!LINK_LIBRARIES! /WHOLEARCHIVE:clang_rt.asan_static_runtime_thunk-i386.lib
		)
	)
)

IF "!_DLL!"=="1" (
	SET "SQUIRREL_DLL_FLAGS=!SQUIRREL_FLAGS! -DSQUIRREL_API=__declspec(dllexport)"
	SET "SQDBG_FLAGS=!SQDBG_FLAGS! -DSQDBG_DLL -DSQDBG_DLL_EXPORT -DSQUIRREL_API=extern"
	SET "SQ_FLAGS=!SQ_FLAGS! -DSQDBG_DLL -DSQUIRREL_API=__declspec(dllimport)"

	IF "!_DEBUG!"=="1" (
		SET COMPILE_FLAGS=!COMPILE_FLAGS! /MDd
	) ELSE (
		SET COMPILE_FLAGS=!COMPILE_FLAGS! /MD
	)

	SET LINK_DLL_FLAGS=!LINK_FLAGS! /DLL
	SET LINK_LIBRARIES=!LINK_LIBRARIES! sqdbg.lib squirrel.lib
) ELSE (
	SET SQUIRREL_DLL_FLAGS=!SQUIRREL_FLAGS!

	IF "!_DEBUG!"=="1" (
		SET COMPILE_FLAGS=!COMPILE_FLAGS! /MTd
	) ELSE (
		SET COMPILE_FLAGS=!COMPILE_FLAGS! /MT
	)

	SET LINK_DLL_FLAGS=!LINK_FLAGS!
	SET LINK_LIBRARIES=!LINK_LIBRARIES! sqdbg.lib squirrel_static.lib
)

IF "!_NOSQSTDLIB!"=="" (
	SET LINK_LIBRARIES=!LINK_LIBRARIES! sqstdlib.lib
) ELSE (
	SET SQ_FLAGS=!SQ_FLAGS! -DNOSQSTDLIB
)

IF "!_UNICODE!"=="1" (
	SET COMPILE_FLAGS=!COMPILE_FLAGS! -DSQUNICODE
)

IF "!_DOUBLE!"=="1" (
	SET COMPILE_FLAGS=!COMPILE_FLAGS! -DSQUSEDOUBLE
)

IF "!_NOGC!"=="1" (
	SET COMPILE_FLAGS=!COMPILE_FLAGS! -DNO_GARBAGE_COLLECTOR
)

IF NOT "!SQVER!"=="" (
	SET COMPILE_FLAGS=!COMPILE_FLAGS! -DSQUIRREL_VERSION_NUMBER=%SQVER%
)

IF "!_FFI!"=="1" (
	IF "%_X64%"=="1" (
		SET SQ_FLAGS=!SQ_FLAGS! -DSQFFI -I%DIR_LIBFFI%\win64\include
		SET LINK_LIBRARIES=!LINK_LIBRARIES! %DIR_LIBFFI%\win64\lib\libffi-%LIBFFI_VERSION%.lib
	) ELSE (
		SET COMPILE_FLAGS=!COMPILE_FLAGS! -DSQFFI -I%DIR_LIBFFI%\win32\include
		SET LINK_LIBRARIES=!LINK_LIBRARIES! %DIR_LIBFFI%\win32\lib\libffi-%LIBFFI_VERSION%.lib
	)
)

IF NOT "!_ASM!"=="" (
	SET COMPILE_FLAGS=!COMPILE_FLAGS! /FA
)

IF "!_CLEAN!"=="1" (
	IF EXIST "%DIR_BUILD%" RD /Q /S "%DIR_BUILD%"
	IF EXIST "%SQ_PATH_BIN%.exe" (
		DEL "%SQ_PATH_BIN%.exe"
		DEL "%SQ_PATH_BIN%.pdb"
	)
	IF EXIST "%DIR_BIN%\libffi-%LIBFFI_VERSION%.dll" DEL "%DIR_BIN%\libffi-%LIBFFI_VERSION%.dll"
)

IF NOT EXIST "%DIR_BUILD%" MKDIR "%DIR_BUILD%"

pushd "%DIR_BUILD%"

ECHO ============ Build sq!TAG! ============

SET "SQUIRREL_SOURCES=""%DIR_SQUIRREL%\squirrel\sqapi.cpp"" ""%DIR_SQUIRREL%\squirrel\sqbaselib.cpp"" ""%DIR_SQUIRREL%\squirrel\sqclass.cpp"" ""%DIR_SQUIRREL%\squirrel\sqcompiler.cpp"" ""%DIR_SQUIRREL%\squirrel\sqdebug.cpp"" ""%DIR_SQUIRREL%\squirrel\sqfuncstate.cpp"" ""%DIR_SQUIRREL%\squirrel\sqlexer.cpp"" ""%DIR_SQUIRREL%\squirrel\sqmem.cpp"" ""%DIR_SQUIRREL%\squirrel\sqobject.cpp"" ""%DIR_SQUIRREL%\squirrel\sqstate.cpp"" ""%DIR_SQUIRREL%\squirrel\sqtable.cpp"" ""%DIR_SQUIRREL%\squirrel\sqvm.cpp"""
SET SQUIRREL_OBJS=sqpcheader.obj sqapi.obj sqbaselib.obj sqclass.obj sqcompiler.obj sqdebug.obj sqfuncstate.obj sqlexer.obj sqmem.obj sqobject.obj sqstate.obj sqtable.obj sqvm.obj

IF NOT EXIST "sqpcheader.cpp" ECHO #include "sqpcheader.h"> sqpcheader.cpp

IF NOT EXIST "squirrel_static.lib" (
	ECHO ------ Compile squirrel ^(static^) ^(%HASH_SQUIRREL%^)

	SET "_CC=CALL %_COMPILE_EXE% %PDB_OUT%""%DIR_BUILD%\squirrel_static"" %COMPILE_FLAGS% %SQUIRREL_FLAGS% /I""%DIR_SQUIRREL%\squirrel"" /Yc""sqpcheader.h"" sqpcheader.cpp"
	!_CC!
	IF ERRORLEVEL 1 GOTO :EX1

	SET "_CC=CALL %_COMPILE_EXE% %PDB_OUT%""%DIR_BUILD%\squirrel_static"" %COMPILE_FLAGS% %SQUIRREL_FLAGS% /Yu""sqpcheader.h"" %SQUIRREL_SOURCES%"
	!_CC!
	IF ERRORLEVEL 1 GOTO :EX1

	ECHO ------ Lib squirrel ^(static^)
	CALL %_LIB_EXE% %LIB_OUT%squirrel_static.lib %LIB_FLAGS% %SQUIRREL_OBJS%
	IF ERRORLEVEL 1 GOTO :EX1
)

IF "%_DLL%"=="1" IF NOT EXIST "squirrel.lib" (
	DEL sqpcheader.pch %SQUIRREL_OBJS%

	ECHO ------ Compile squirrel ^(%HASH_SQUIRREL%^)

	SET "_CC=CALL %_COMPILE_EXE% %PDB_OUT%""%DIR_BUILD%\squirrel"" %COMPILE_FLAGS% %SQUIRREL_DLL_FLAGS% /I""%DIR_SQUIRREL%\squirrel"" /Yc""sqpcheader.h"" sqpcheader.cpp"
	!_CC!
	IF ERRORLEVEL 1 GOTO :EX1

	SET "_CC=CALL %_COMPILE_EXE% %PDB_OUT%""%DIR_BUILD%\squirrel"" %COMPILE_FLAGS% %SQUIRREL_DLL_FLAGS% /Yu""sqpcheader.h"" %SQUIRREL_SOURCES%"
	!_CC!
	IF ERRORLEVEL 1 GOTO :EX1

	ECHO ------ Lib squirrel
	CALL %_LINK_EXE% %LINK_OUT%squirrel.dll %LINK_DLL_FLAGS% %SQUIRREL_OBJS%
	IF ERRORLEVEL 1 GOTO :EX1
)

IF "!_NOSQSTDLIB!"=="" IF NOT EXIST "sqstdlib.lib" (
	ECHO ------ Compile sqstdlib ^(%HASH_SQUIRREL%^)
	SET "_CC=CALL %_COMPILE_EXE% %COMPILE_FLAGS% %SQUIRREL_DLL_FLAGS% ""%DIR_SQUIRREL%\sqstdlib\sqstdaux.cpp"" ""%DIR_SQUIRREL%\sqstdlib\sqstdblob.cpp"" ""%DIR_SQUIRREL%\sqstdlib\sqstdio.cpp"" ""%DIR_SQUIRREL%\sqstdlib\sqstdmath.cpp"" ""%DIR_SQUIRREL%\sqstdlib\sqstdrex.cpp"" ""%DIR_SQUIRREL%\sqstdlib\sqstdstream.cpp"" ""%DIR_SQUIRREL%\sqstdlib\sqstdstring.cpp"" ""%DIR_SQUIRREL%\sqstdlib\sqstdsystem.cpp"""
	!_CC!
	IF ERRORLEVEL 1 GOTO :EX1

	ECHO ------ Lib sqstdlib
	IF "%_DLL%"=="1" (
		CALL %_LINK_EXE% %LINK_OUT%sqstdlib.dll %LINK_DLL_FLAGS% sqstdaux.obj sqstdblob.obj sqstdio.obj sqstdmath.obj sqstdrex.obj sqstdstream.obj sqstdstring.obj sqstdsystem.obj squirrel.lib
		IF ERRORLEVEL 1 GOTO :EX1
	) ELSE (
		CALL %_LIB_EXE% %LIB_OUT%sqstdlib.lib %LIB_FLAGS% sqstdaux.obj sqstdblob.obj sqstdio.obj sqstdmath.obj sqstdrex.obj sqstdstream.obj sqstdstring.obj sqstdsystem.obj
		IF ERRORLEVEL 1 GOTO :EX1
	)
)

IF NOT EXIST "sqdbg.lib" (
	ECHO ------ Compile sqdbg ^(%HASH_SQDBG%^)
	CALL %_COMPILE_EXE% %COMPILE_OUT%"sqdbg.obj" %PDB_OUT%"%DIR_BUILD%\sqdbg" %COMPILE_FLAGS% %SQDBG_FLAGS% "%DIR_SQDBG%\sqdbg\server.cpp"
	IF ERRORLEVEL 1 GOTO :EX1

	ECHO ------ Lib sqdbg
	IF "%_DLL%"=="1" (
		CALL %_LINK_EXE% %LINK_OUT%sqdbg.dll %LINK_DLL_FLAGS% sqdbg.obj squirrel_static.lib
		IF ERRORLEVEL 1 GOTO :EX1
	) ELSE (
		CALL %_LIB_EXE% %LIB_OUT%sqdbg.lib %LIB_FLAGS% sqdbg.obj
		IF ERRORLEVEL 1 GOTO :EX1
	)
)

ECHO ------ Compile sq!TAG! ^(%HASH_SQ%^)
SET "_CC=CALL !_COMPILE_EXE! %COMPILE_OUT%""%DIR_BUILD%\sq.obj"" %PDB_OUT%""%DIR_BUILD%\sq"" !COMPILE_FLAGS! !SQ_FLAGS! %WORKDIR%\sq.cpp"
!_CC!
IF ERRORLEVEL 1 GOTO :EX1

ECHO ------ Link sq!TAG!
CALL !_LINK_EXE! %LINK_OUT%"%SQ_PATH_BIN%.exe" !LINK_FLAGS! sq.obj !LINK_LIBRARIES!
IF ERRORLEVEL 1 GOTO :EX1

IF "!_DLL!"=="1" (
	COPY squirrel.dll "%DIR_BIN%\squirrel.dll"
	IF "!_NOSQSTDLIB!"=="" COPY sqstdlib.dll "%DIR_BIN%\sqstdlib.dll"
	COPY sqdbg.dll "%DIR_BIN%\sqdbg.dll"
)

IF "!_FFI!"=="1" (
	IF "%_X64%"=="1" (
		COPY "%DIR_LIBFFI%\win64\lib\libffi-%LIBFFI_VERSION%.dll" "%DIR_BIN%\libffi-%LIBFFI_VERSION%.dll"
	) ELSE (
		COPY "%DIR_LIBFFI%\win32\lib\libffi-%LIBFFI_VERSION%.dll" "%DIR_BIN%\libffi-%LIBFFI_VERSION%.dll"
	)
)

DEL *.obj
popd

ECHO output: %SQ_PATH_BIN%.exe

ENDLOCAL & (
	SET SQ_PATH_BIN=%SQ_PATH_BIN%
	SET LIBFFI_VERSION=%LIBFFI_VERSION%
)

IF "%_AUTOEXIT%"=="1" EXIT 0
GOTO :EOF

:EX1
ENDLOCAL
EXIT /B 1

