@echo off
setlocal
cd /d "%~dp0"
rem Builds Pygmalion (Release), dist\Pygmalion-win64.zip (portable) and dist\Pygmalion-setup.exe (the installer).
rem Needs Visual Studio 2022 (C++), CMake and Inno Setup 6 (for the installer). SDL2 and MakeHuman's asset pack (CC0)
rem are fetched the first time.
if not defined BUILD_DIR set "BUILD_DIR=build"
if not exist "external\SDL2-2.30.9\cmake" (
    echo Getting SDL2's development package, once...
    if not exist external mkdir external
    curl -sL -o external\SDL2-devel.zip https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-devel-2.30.9-VC.zip || goto :error
    tar -xf external\SDL2-devel.zip -C external || goto :error
    del external\SDL2-devel.zip
)
if not exist "assets-mh\clothes" (
    echo Getting MakeHuman's asset pack ^(clothes, hair, skins: CC0^), once...
    if not exist external mkdir external
    curl -L -o external\makehuman_system_assets_cc0.zip https://mirror1.makehuman.net/asset_packs/makehuman_system_assets/makehuman_system_assets_cc0.zip || goto :error
    if not exist external\mh-assets mkdir external\mh-assets
    tar -xf external\makehuman_system_assets_cc0.zip -C external\mh-assets || goto :error
    if exist external\mh-assets\system ( xcopy /e /i /q /y external\mh-assets\system assets-mh >nul ) else ( xcopy /e /i /q /y external\mh-assets assets-mh >nul )
)
cmake -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build %BUILD_DIR% --config Release || goto :error
if exist dist\Pygmalion rmdir /s /q dist\Pygmalion
mkdir dist\Pygmalion
copy /y %BUILD_DIR%\Release\Pygmalion.exe dist\Pygmalion\ >nul || goto :error
copy /y %BUILD_DIR%\Release\SDL2.dll dist\Pygmalion\ >nul || goto :error
xcopy /e /i /q /y %BUILD_DIR%\Release\assets dist\Pygmalion\assets >nul || goto :error
xcopy /e /i /q /y data dist\Pygmalion\data >nul || goto :error
xcopy /e /i /q /y assets-mh dist\Pygmalion\assets-mh >nul || goto :error
for %%f in (README.md LICENSE.md NOTICE install.bat install.ps1) do if exist %%f copy /y %%f dist\Pygmalion\ >nul
if exist dist\Pygmalion-win64.zip del dist\Pygmalion-win64.zip
"%SystemRoot%\System32\tar.exe" -a -c -f dist\Pygmalion-win64.zip -C dist Pygmalion || goto :error
set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if exist "%ISCC%" ( "%ISCC%" /Q installer\pygmalion.iss || goto :error ) else echo Inno Setup 6 isn't installed: no installer, the zip is there.
echo Built: dist\Pygmalion-win64.zip and dist\Pygmalion-setup.exe
rem installed on this PC: this build replaces that copy too
if exist "%LOCALAPPDATA%\Programs\Pygmalion\Pygmalion.exe" call "%~dp0install.bat" --quiet
exit /b 0
:error
echo Build failed, see above.
pause
exit /b 1
