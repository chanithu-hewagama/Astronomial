@echo off
setlocal
set "MSYS=D:\Programmes\msys2"
if not exist "%MSYS%\usr\bin\bash.exe" (
  echo Could not find MSYS2 at %MSYS%. Edit the MSYS line at the top of this file.
  pause
  exit /b 1
)
set MSYSTEM=UCRT64
"%MSYS%\usr\bin\bash.exe" -lc "cd \"$(cygpath -u '%~dp0')\" && bash build.sh"
if errorlevel 1 (
  echo Build failed. Run this file again to resume after fixing the error.
  pause
  exit /b 1
)
set "PATH=%MSYS%\ucrt64\bin;%PATH%"
if not "%~1"=="" (
  astronomial.exe %*
) else (
  astronomial.exe
)
pause