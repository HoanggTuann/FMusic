; Trình cài đặt FMusic (Inno Setup 6). Build: ISCC.exe /DMyAppVersion=0.3.0 installer\FMusic.iss
#ifndef MyAppVersion
  #define MyAppVersion "0.3.0"
#endif

[Setup]
AppId={{815A5495-3B4E-42FA-9EA6-E8275BA64B06}
AppName=FMusic
AppVersion={#MyAppVersion}
AppPublisher=FMusic
DefaultDirName={autopf}\FMusic
DefaultGroupName=FMusic
DisableProgramGroupPage=yes
OutputDir=..\installer-out
OutputBaseFilename=FMusic-Setup-{#MyAppVersion}
SetupIconFile=..\windows\icon\fmusic.ico
UninstallDisplayIcon={app}\FMusic.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\windows\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\FMusic"; Filename: "{app}\FMusic.exe"
Name: "{autodesktop}\FMusic"; Filename: "{app}\FMusic.exe"; Tasks: desktopicon

[Registry]
; Hiện FMusic trong menu "Mở bằng" cho tệp nhạc
Root: HKA; Subkey: "Software\Classes\Applications\FMusic.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "FMusic"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\FMusic.exe\shell\open\command"; ValueType: string; ValueData: """{app}\FMusic.exe"" ""%1"""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\FMusic.exe\SupportedTypes"; ValueType: string; ValueName: ".mp3"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\FMusic.exe\SupportedTypes"; ValueType: string; ValueName: ".flac"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\FMusic.exe\SupportedTypes"; ValueType: string; ValueName: ".m4a"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\FMusic.exe\SupportedTypes"; ValueType: string; ValueName: ".wav"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\FMusic.exe\SupportedTypes"; ValueType: string; ValueName: ".ogg"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\FMusic.exe\SupportedTypes"; ValueType: string; ValueName: ".opus"; ValueData: ""

[Run]
Filename: "{app}\FMusic.exe"; Description: "{cm:LaunchProgram,FMusic}"; Flags: nowait postinstall skipifsilent
