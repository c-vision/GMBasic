@echo off
rem tools/sh/zigcc.cmd - wrapper del compilatore C per Windows (equivalente di zigcc.sh).
rem Uso: zigcc.cmd -c file.c -o file.o   /   cmake -DCMAKE_C_COMPILER=<repo>\tools\sh\zigcc.cmd
rem
rem Risoluzione (nessuna variabile d'ambiente per controllare il comportamento):
rem   1. %1 == --zig <path>   percorso esplicito (consumato da questo wrapper);
rem   2. %USERPROFILE%\.gmbasic\toolchains\zig\<versione>-<rid>\zig.exe
rem   3. zig nel PATH;
rem   4. errore esplicito.
rem Cache (D8): %USERPROFILE%\.gmbasic\toolchains\zig-cache via --cache-dir.
setlocal enabledelayedexpansion

set "ZIG_VERSION=%~2"
set "ZIG_ARG="
if "%~1"=="--zig" (
  set "ZIG_ARG=%~2"
  set "ZIG_VERSION=0.17.0"
  shift
  shift
) else (
  set "ZIG_VERSION=0.17.0"
)
rem Se --zig non e' stato passato, l'eventuale --zig <path> e' gia' stato letto sopra.

set "TC=%USERPROFILE%\.gmbasic\toolchains\zig"
set "CACHE=%USERPROFILE%\.gmbasic\toolchains\zig-cache"
if not exist "%CACHE%" mkdir "%CACHE%" >nul 2>&1

set "ZIG="
if defined ZIG_ARG (
  if exist "%ZIG_ARG%" set "ZIG=%ZIG_ARG%"
) else (
  if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    if exist "%TC%\%ZIG_VERSION%-win-arm64\zig.exe" set "ZIG=%TC%\%ZIG_VERSION%-win-arm64\zig.exe"
  ) else (
    if exist "%TC%\%ZIG_VERSION%-win-x64\zig.exe" set "ZIG=%TC%\%ZIG_VERSION%-win-x64\zig.exe"
  )
  if not defined ZIG for /d %%D in ("%TC%\%ZIG_VERSION%-*") do if exist "%%D\zig.exe" set "ZIG=%%D\zig.exe"
  if not defined ZIG for %%Z in (zig.exe) do if not "%%~$PATH:Z"=="" set "ZIG=%%~$PATH:Z"
)

if not defined ZIG (
  echo zigcc: nessuna toolchain zig %ZIG_VERSION% trovata.>&2
  echo   cercata in: %TC%\%ZIG_VERSION%-win-*>&2
  echo   rimedio:    gmb toolchain ensure>&2
  exit /b 2
)

"%ZIG%" cc --cache-dir "%CACHE%" %*
exit /b %ERRORLEVEL%
