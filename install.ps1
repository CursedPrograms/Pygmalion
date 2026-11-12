# Installs Pygmalion for this user (%LOCALAPPDATA%\Programs\Pygmalion, Start menu + Desktop shortcuts), through install.bat.
& cmd /c "`"$PSScriptRoot\install.bat`" $args"
exit $LASTEXITCODE
