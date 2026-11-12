# Builds Pygmalion (Release), dist\Pygmalion-win64.zip and dist\Pygmalion-setup.exe, through build.bat.
$ErrorActionPreference = 'Stop'
& cmd /c "`"$PSScriptRoot\build.bat`""
exit $LASTEXITCODE
