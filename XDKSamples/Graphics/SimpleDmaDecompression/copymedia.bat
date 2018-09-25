:: %1 MediaList.txt file
:: %2 destination $(OutDir)
:: %3 source Sample Media Dir

@echo off

if not exist %~1 (
::  echo MediaList %~1 isn't found, skipping Copying Media step
    goto :EOF
)

SETLOCAL ENABLEDELAYEDEXPANSION


SET OUTDIR_SAMPLE=%~2
SET SAMPLE_MEDIA_DIR=%~3

echo Copying Media listed in %1
echo Sample media dir = %SAMPLE_MEDIA_DIR%
echo Destination sample = !OUTDIR_SAMPLE!

for /f "usebackq tokens=1-4" %%i in (%~1) do (
	if     "%%i"=="+" echo "%%j\%%l"  To  "%%k" && copy  "%%j\%%l" "%%k" /y > NUL
    if NOT "%%i"=="+" robocopy  "%%i" "%%j" "%%k" /s /DCOPY:T /a-:RA /MT /sl /xo /v /r:5 /ts /np /njh /njs /ndl
    if %errorlevel% GTR 3 goto fail
)

:fail

SET OUTDIR_SAMPLE=
SET SAMPLE_MEDIA_DIR=

ENDLOCAL

if %errorlevel% LEQ 3 exit /B 0