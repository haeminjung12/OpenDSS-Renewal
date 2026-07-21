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
#ifndef VcRedist
  #error VcRedist not defined. Public installers must embed the official Microsoft x64 runtime.
#endif
#ifndef VcRedistVersion
  #error VcRedistVersion not defined.
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
Source: "{#VcRedist}"; DestDir: "{tmp}"; DestName: "vc_redist.x64.exe"; Flags: ignoreversion deleteafterinstall

[Code]
var
  VcRestartRequired: Boolean;

function VcRedistRequired: Boolean;
var
  Installed: Cardinal;
  Major, Minor, Build, Revision: Cardinal;
begin
  Result := True;
  Installed := 0;
  if RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and
     (Installed = 1) and
     RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Major', Major) and
     RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Minor', Minor) and
     RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Bld', Build) and
     RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Rbld', Revision) then
  begin
    Result := ComparePackedVersion(
      PackVersionComponents(Major, Minor, Build, Revision),
      {#StrToVersion(VcRedistVersion)}) < 0;
    if Result then
      Log('Microsoft VC++ x64 runtime detected: ' + IntToStr(Major) + '.' + IntToStr(Minor) + '.' +
        IntToStr(Build) + '.' + IntToStr(Revision) + '; required: {#VcRedistVersion}; install required: True')
    else
      Log('Microsoft VC++ x64 runtime detected: ' + IntToStr(Major) + '.' + IntToStr(Minor) + '.' +
        IntToStr(Build) + '.' + IntToStr(Revision) + '; required: {#VcRedistVersion}; install required: False');
  end;
  if Result and (Installed <> 1) then
    Log('Microsoft VC++ x64 runtime is absent or incomplete; installation is required.');
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ExitCode: Integer;
begin
  if (CurStep = ssPostInstall) and VcRedistRequired then
  begin
    WizardForm.StatusLabel.Caption := 'Installing required Microsoft components...';
    if not Exec(ExpandConstant('{tmp}\vc_redist.x64.exe'), '/install /quiet /norestart', '', SW_HIDE,
      ewWaitUntilTerminated, ExitCode) then
      RaiseException('The Microsoft Visual C++ runtime installer could not be started.');

    if ExitCode = 3010 then
      VcRestartRequired := True
    else if (ExitCode <> 0) and (ExitCode <> 1638) then
      RaiseException(Format('The Microsoft Visual C++ runtime installation failed (exit code %d).', [ExitCode]));
  end
  else if CurStep = ssPostInstall then
    Log('Microsoft VC++ x64 runtime already satisfies the required version; bundled installer skipped.');
end;

function NeedRestart: Boolean;
begin
  Result := VcRestartRequired;
end;

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
