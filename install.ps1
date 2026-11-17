# Installs Pygmalion for this user (%LOCALAPPDATA%\Programs\Pygmalion, Start menu and Desktop shortcuts named "Atelier - Pygmalion"), through install.bat.
& cmd /c "`"$PSScriptRoot\install.bat`" $args"
exit $LASTEXITCODE
