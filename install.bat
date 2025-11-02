call build double %*
if errorlevel 1 exit /B 1

SET "INSTALLPATH=%USERPROFILE%\bin"

copy "%SQ_PATH_BIN%.exe" "%INSTALLPATH%\sq.exe"

if exist "%SQ_PATH_BIN%\..\squirrel.dll" (
	copy "%SQ_PATH_BIN%\..\squirrel.dll" "%INSTALLPATH%\squirrel.dll"
	copy "%SQ_PATH_BIN%\..\sqstdlib.dll" "%INSTALLPATH%\sqstdlib.dll"
	copy "%SQ_PATH_BIN%\..\sqdbg.dll" "%INSTALLPATH%\sqdbg.dll"
)

if exist "%SQ_PATH_BIN%\..\libffi-%LIBFFI_VERSION%.dll" (
	copy "%SQ_PATH_BIN%\..\libffi-%LIBFFI_VERSION%.dll" "%INSTALLPATH%\libffi-%LIBFFI_VERSION%.dll"
)

if exist "%SQ_PATH_BIN%.pdb" (
	copy "%SQ_PATH_BIN%.pdb" "%INSTALLPATH%\sq.pdb"
) else if exist "%INSTALLPATH%\sq.pdb" (
	del "%INSTALLPATH%\sq.pdb"
)
