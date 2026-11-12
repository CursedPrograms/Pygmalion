@echo off
setlocal
cd /d "%~dp0"
rem Installs Pygmalion for this user: %LOCALAPPDATA%\Programs\Pygmalion, with Start menu and Desktop shortcuts.
rem Run build.bat first (or unzip a release and run this beside it).
set "SRC=dist\Pygmalion"
if exist "Pygmalion.exe" set "SRC=."
if not exist "%SRC%\Pygmalion.exe" ( echo Build it first: build.bat & exit /b 1 )
tasklist /fi "imagename eq Pygmalion.exe" | find /i "Pygmalion.exe" >nul
if not errorlevel 1 ( echo Pygmalion is open: close it and run this again. & exit /b 1 )
set "INST=%LOCALAPPDATA%\Programs\Pygmalion"
if not exist "%INST%" mkdir "%INST%"
xcopy /e /i /q /y "%SRC%\*" "%INST%\" >nul || ( echo Couldn't copy to %INST% & exit /b 1 )
powershell -NoProfile -Command "$s=New-Object -ComObject WScript.Shell; foreach($d in @([Environment]::GetFolderPath('Programs'),[Environment]::GetFolderPath('Desktop'))){ $l=$s.CreateShortcut((Join-Path $d 'Pygmalion.lnk')); $l.TargetPath='%INST%\Pygmalion.exe'; $l.WorkingDirectory='%INST%'; $l.IconLocation='%INST%\Pygmalion.exe,0'; $l.Description='Pygmalion: people in 3D with sliders'; $l.Save() }"
echo Installed: %INST%
if /i not "%~1"=="--quiet" echo Start it from the Start menu or the Desktop.
exit /b 0
