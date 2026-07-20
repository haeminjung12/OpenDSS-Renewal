#ifndef SourceDir
  #error SourceDir not defined. Use /DSourceDir="C:\path\to\portable\folder"
#endif

#define AppName "OpenDSS"
#define AppVersion "0.9.0"
#define AppPublisher "haeminjung"
#define AppExeName "OpenDSS.exe"
#define DefaultDirName "{pf}\OpenDSS"
#ifndef OutputDir
  #define OutputDir SourcePath + "\output"
#endif

[Setup]
AppId={{0A2C6D54-74A9-4A7C-9B55-0878E1A5CE9B}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={#DefaultDirName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=OpenDSSSetup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#AppExeName}
InfoBeforeFile=preinstall_note.txt

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
