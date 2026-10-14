@echo off
setlocal
set "MSYS=D:\Programmes\msys2"
if not exist "%MSYS%\usr\bin\bash.exe" (
  echo MSYS2 not found at %MSYS%. Edit MSYS in this file.
  exit /b 1
)
set "PROJECT=%~dp0"
set "MSYSTEM=UCRT64"
"%MSYS%\usr\bin\bash.exe" -lc "cd \"$(cygpath -u '%PROJECT%')\" && bash prepare-adddata.sh"
if errorlevel 1 exit /b 1
echo.
echo adddata is ready for Inno Setup.
pause
