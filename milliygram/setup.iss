; MilliyGram Desktop — Windows o'rnatuvchisi (Inno Setup 6)
; ISCC /DMyAppVersion=1.0.0 /DDistPath=... /DIconPath=... setup.iss

#define MyAppName "MilliyGram"
#define MyAppExe "MilliyGram.exe"

[Setup]
AppId={{8E1A2C47-5B3D-4F6E-9A71-2D4C6B8E0F13}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=MilliyGram
AppPublisherURL=https://t.me/MilliyGramm
AppSupportURL=https://t.me/MilliyGramChat
DefaultDirName={userappdata}\MilliyGram Desktop
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
OutputBaseFilename=MilliyGram-Setup-x64
SetupIconFile={#IconPath}
UninstallDisplayIcon={app}\{#MyAppExe}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=force

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#DistPath}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExe}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
