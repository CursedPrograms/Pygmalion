; Pygmalion installer (Inno Setup 6), built by build.bat from dist\Pygmalion.
; For you only (no administrator needed), into %LOCALAPPDATA%\Programs\Pygmalion - the folder the Atelier launcher
; uses too - with Start menu and Desktop shortcuts; MakeHuman's body, shapes and asset pack are inside.

#define AppName "Pygmalion"
#define AppVersion "1.0.0"
#define AppPublisher "Cursed Entertainment"
#define AppURL "https://cursedprograms.github.io/Pygmalion/"

[Setup]
AppId={{30C41005-C5F8-461A-878D-A91C1BFA4ACE}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL=https://github.com/CursedPrograms/Pygmalion/issues
AppUpdatesURL=https://github.com/CursedPrograms/Pygmalion/releases
DefaultDirName={localappdata}\Programs\Pygmalion
DefaultGroupName=Pygmalion
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
LicenseFile=..\LICENSE.md
SetupIconFile=..\assets\icon.ico
UninstallDisplayIcon={app}\Pygmalion.exe
UninstallDisplayName=Pygmalion (people in 3D)
WizardStyle=modern
OutputDir=..\dist
OutputBaseFilename=Pygmalion-setup
Compression=lzma2/ultra64
SolidCompression=yes
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "A shortcut on the Desktop"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\dist\Pygmalion\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Pygmalion"; Filename: "{app}\Pygmalion.exe"; Comment: "People in 3D with sliders"
Name: "{group}\Uninstall Pygmalion"; Filename: "{uninstallexe}"
Name: "{userdesktop}\Pygmalion"; Filename: "{app}\Pygmalion.exe"; Comment: "People in 3D with sliders"; Tasks: desktopicon

[Run]
Filename: "{app}\Pygmalion.exe"; Description: "Start Pygmalion now"; Flags: nowait postinstall skipifsilent

[Messages]
WelcomeLabel2=This puts Pygmalion on your PC: it makes people in 3D with sliders - shape them, dress them, pose them and take them into any 3D program.%n%nEverything it needs comes with it.
