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
DisableDirPage=yes
DisableReadyPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=OpenDSSSetup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#AppExeName}

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#VcRedist}"; DestDir: "{tmp}"; DestName: "vc_redist.x64.exe"; Flags: ignoreversion deleteafterinstall

[Code]
const
  StageWelcome = 'Welcome';
  StagePrerequisiteCheck = 'Prerequisite Check';
  StageOpenDssInstallation = 'OpenDSS installation';
  StageTrainingEnvironmentSetup = 'Training Environment setup';
  StageFinalVerification = 'Final Verification';
  DcamDownloadUrl = 'https://www.hamamatsu.com/us/en/product/cameras/software/driver-software/dcam-api-for-windows.html';
  NiDownloadUrl = 'https://www.ni.com/en/support/downloads/drivers/download.ni-daq-mx.html/';

var
  VcRestartRequired: Boolean;
  TrainingProvisioned: Boolean;
  TrainingFailureDetail: String;
  PrerequisitePage: TWizardPage;
  DcamStatusLabel: TNewStaticText;
  NiStatusLabel: TNewStaticText;
  NvidiaStatusLabel: TNewStaticText;
  InternetStatusLabel: TNewStaticText;
  VcStatusLabel: TNewStaticText;
  DcamDownloadButton: TNewButton;
  NiDownloadButton: TNewButton;
  CheckAgainButton: TNewButton;
  RepairTrainingButton: TNewButton;

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

function InternetAvailable: Boolean;
var
  Request: Variant;
  StatusCode: Integer;
begin
  Result := False;
  try
    Request := CreateOleObject('WinHttp.WinHttpRequest.5.1');
    Request.SetTimeouts(3000, 3000, 3000, 3000);
    Request.Open('HEAD', 'https://www.python.org/', False);
    Request.Send('');
    StatusCode := Request.Status;
    Result := (StatusCode >= 200) and (StatusCode < 500);
  except
    Log('Internet connectivity check failed: ' + GetExceptionMessage);
  end;
end;

procedure RefreshPrerequisites;
begin
  if FileExists(ExpandConstant('{sys}\dcamapi.dll')) then
    DcamStatusLabel.Caption := 'DCAM (dcamapi.dll): Ready'
  else
    DcamStatusLabel.Caption := 'DCAM (dcamapi.dll): Driver Required';

  if FileExists(ExpandConstant('{sys}\nicaiu.dll')) then
    NiStatusLabel.Caption := 'NI-DAQmx (nicaiu.dll): Ready'
  else
    NiStatusLabel.Caption := 'NI-DAQmx (nicaiu.dll): Driver Required';

  if FileExists(ExpandConstant('{sys}\nvcuda.dll')) then
    NvidiaStatusLabel.Caption :=
      'NVIDIA/GPU: CUDA driver detected; compatibility is verified during Training setup'
  else
    NvidiaStatusLabel.Caption := 'NVIDIA/GPU: Not detected; CPU fallback is available';

  if InternetAvailable then
    InternetStatusLabel.Caption := 'Internet connectivity: Ready'
  else
    InternetStatusLabel.Caption :=
      'Internet connectivity: Unavailable; Training setup requires internet';

  if VcRedistRequired then
    VcStatusLabel.Caption :=
      'Windows runtime: Microsoft VC++ x64 update will be installed'
  else
    VcStatusLabel.Caption := 'Windows runtime: Microsoft VC++ x64 is ready';
end;

procedure OpenDcamDownloadPage(Sender: TObject);
var
  ErrorCode: Integer;
begin
  if not ShellExec('open', DcamDownloadUrl, '', '', SW_SHOWNORMAL,
      ewNoWait, ErrorCode) then
    MsgBox('The official Hamamatsu DCAM download page could not be opened.',
      mbError, MB_OK);
end;

procedure OpenNiDownloadPage(Sender: TObject);
var
  ErrorCode: Integer;
begin
  if not ShellExec('open', NiDownloadUrl, '', '', SW_SHOWNORMAL,
      ewNoWait, ErrorCode) then
    MsgBox('The official NI-DAQmx download page could not be opened.',
      mbError, MB_OK);
end;

procedure CheckPrerequisitesAgain(Sender: TObject);
begin
  RefreshPrerequisites;
end;

function TrainingProvisionerParameters: String;
begin
  Result :=
    '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' +
    ExpandConstant('{app}\training\python\scripts\windows\provision-training-runtime.ps1') +
    '" -BootstrapRoot "' + ExpandConstant('{app}\training\bootstrap') +
    '" -InstallRoot "' + ExpandConstant('{localappdata}\OpenDSS') +
    '" -CheckOutput "' +
    ExpandConstant('{localappdata}\OpenDSS\training-runtime-check') + '"';
end;

function RunTrainingProvisioner: Boolean;
var
  ExitCode: Integer;
begin
  TrainingProvisioned := False;
  TrainingFailureDetail := '';
  Result := Exec(
    ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    TrainingProvisionerParameters, '', SW_HIDE, ewWaitUntilTerminated, ExitCode);
  if not Result then
  begin
    TrainingFailureDetail := 'the Training bootstrap could not be started';
    Log('OpenDSS Training setup unavailable: ' + TrainingFailureDetail);
    Exit;
  end;

  if ExitCode <> 0 then
  begin
    TrainingFailureDetail := Format(
      'setup failed with exit code %d; the previous accepted runtime, if any, was preserved', [ExitCode]);
    Log('OpenDSS Training setup unavailable: ' + TrainingFailureDetail);
    Result := False;
    Exit;
  end;

  TrainingProvisioned := True;
  Result := True;
end;

function ProbeTrainingCompute(var ComputeStatus: String): Boolean;
var
  PythonPath: String;
  ExitCode: Integer;
begin
  Result := False;
  PythonPath :=
    ExpandConstant('{localappdata}\OpenDSS\training-venv-gpu\Scripts\python.exe');
  if not FileExists(PythonPath) then
  begin
    ComputeStatus := 'Unavailable';
    Exit;
  end;

  if not Exec(PythonPath,
      '-I -c "import sys,torch,onnxruntime as ort;sys.exit(0 if torch.cuda.is_available() and ''CUDAExecutionProvider'' in ort.get_available_providers() else 10)"',
      '', SW_HIDE, ewWaitUntilTerminated, ExitCode) then
  begin
    ComputeStatus := 'Unavailable';
    Exit;
  end;

  if ExitCode = 0 then
  begin
    ComputeStatus := 'CUDA';
    Result := True;
  end
  else if ExitCode = 10 then
  begin
    ComputeStatus := 'CPU fallback';
    Result := True;
  end
  else
    ComputeStatus := 'Unavailable';
end;

procedure UpdateFinalVerification;
var
  VerificationText: String;
  ComputeStatus: String;
  ComputeVerified: Boolean;
begin
  VerificationText := 'OpenDSS: ';
  if FileExists(ExpandConstant('{app}\{#AppExeName}')) then
    VerificationText := VerificationText + 'Installed'
  else
    VerificationText := VerificationText + 'Unavailable';

  if FileExists(ExpandConstant('{sys}\dcamapi.dll')) then
    VerificationText := VerificationText + #13#10 + 'DCAM: Ready'
  else
    VerificationText := VerificationText + #13#10 + 'DCAM: Driver Required';

  if FileExists(ExpandConstant('{sys}\nicaiu.dll')) then
    VerificationText := VerificationText + #13#10 + 'NI-DAQ: Ready'
  else
    VerificationText := VerificationText + #13#10 + 'NI-DAQ: Driver Required';

  ComputeVerified := False;
  if TrainingProvisioned then
    ComputeVerified := ProbeTrainingCompute(ComputeStatus)
  else
    ComputeStatus := 'Unavailable';

  if TrainingProvisioned and ComputeVerified then
  begin
    VerificationText := VerificationText + #13#10 + 'Training: Ready';
    VerificationText := VerificationText + #13#10 +
      'Training compute: ' + ComputeStatus;
    RepairTrainingButton.Visible := False;
  end
  else
  begin
    TrainingProvisioned := False;
    VerificationText := VerificationText + #13#10 + 'Training: Unavailable';
    VerificationText := VerificationText + #13#10 +
      'Training compute: Unavailable';
    if TrainingFailureDetail <> '' then
      VerificationText := VerificationText + #13#10 + TrainingFailureDetail;
    RepairTrainingButton.Visible := True;
  end;

  WizardForm.FinishedHeadingLabel.Caption := StageFinalVerification;
  WizardForm.FinishedLabel.Caption := VerificationText;
end;

procedure RepairTrainingEnvironment(Sender: TObject);
begin
  RepairTrainingButton.Enabled := False;
  WizardForm.FinishedLabel.Caption :=
    StageTrainingEnvironmentSetup + ': downloading and verifying...';
  WizardForm.Repaint;
  RunTrainingProvisioner;
  UpdateFinalVerification;
  RepairTrainingButton.Enabled := True;
end;

procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel1.Caption := StageWelcome;
  PrerequisitePage := CreateCustomPage(
    wpWelcome, StagePrerequisiteCheck,
    'Review detected hardware, network, and Windows runtime prerequisites.');

  DcamStatusLabel := TNewStaticText.Create(PrerequisitePage);
  DcamStatusLabel.Parent := PrerequisitePage.Surface;
  DcamStatusLabel.SetBounds(0, ScaleY(8), PrerequisitePage.SurfaceWidth, ScaleY(18));

  DcamDownloadButton := TNewButton.Create(PrerequisitePage);
  DcamDownloadButton.Parent := PrerequisitePage.Surface;
  DcamDownloadButton.Caption := 'Open official DCAM download page';
  DcamDownloadButton.SetBounds(0, ScaleY(30), ScaleX(220), ScaleY(24));
  DcamDownloadButton.OnClick := @OpenDcamDownloadPage;

  NiStatusLabel := TNewStaticText.Create(PrerequisitePage);
  NiStatusLabel.Parent := PrerequisitePage.Surface;
  NiStatusLabel.SetBounds(0, ScaleY(66), PrerequisitePage.SurfaceWidth, ScaleY(18));

  NiDownloadButton := TNewButton.Create(PrerequisitePage);
  NiDownloadButton.Parent := PrerequisitePage.Surface;
  NiDownloadButton.Caption := 'Open official NI-DAQmx download page';
  NiDownloadButton.SetBounds(0, ScaleY(88), ScaleX(220), ScaleY(24));
  NiDownloadButton.OnClick := @OpenNiDownloadPage;

  NvidiaStatusLabel := TNewStaticText.Create(PrerequisitePage);
  NvidiaStatusLabel.Parent := PrerequisitePage.Surface;
  NvidiaStatusLabel.SetBounds(0, ScaleY(124), PrerequisitePage.SurfaceWidth, ScaleY(18));

  InternetStatusLabel := TNewStaticText.Create(PrerequisitePage);
  InternetStatusLabel.Parent := PrerequisitePage.Surface;
  InternetStatusLabel.SetBounds(0, ScaleY(148), PrerequisitePage.SurfaceWidth, ScaleY(18));

  VcStatusLabel := TNewStaticText.Create(PrerequisitePage);
  VcStatusLabel.Parent := PrerequisitePage.Surface;
  VcStatusLabel.SetBounds(0, ScaleY(172), PrerequisitePage.SurfaceWidth, ScaleY(18));

  CheckAgainButton := TNewButton.Create(PrerequisitePage);
  CheckAgainButton.Parent := PrerequisitePage.Surface;
  CheckAgainButton.Caption := 'Check Again';
  CheckAgainButton.SetBounds(0, ScaleY(204), ScaleX(100), ScaleY(24));
  CheckAgainButton.OnClick := @CheckPrerequisitesAgain;

  RepairTrainingButton := TNewButton.Create(WizardForm);
  RepairTrainingButton.Parent := WizardForm.FinishedPage;
  RepairTrainingButton.Caption := 'Repair Training Environment';
  RepairTrainingButton.SetBounds(ScaleX(176), ScaleY(210), ScaleX(180), ScaleY(26));
  RepairTrainingButton.OnClick := @RepairTrainingEnvironment;
  RepairTrainingButton.Visible := False;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = PrerequisitePage.ID then
    RefreshPrerequisites
  else if CurPageID = wpInstalling then
  begin
    WizardForm.PageNameLabel.Caption := StageOpenDssInstallation;
    WizardForm.StatusLabel.Caption := StageOpenDssInstallation;
  end
  else if CurPageID = wpFinished then
    UpdateFinalVerification;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ExitCode: Integer;
begin
  if CurStep = ssInstall then
  begin
    WizardForm.PageNameLabel.Caption := StageOpenDssInstallation;
    WizardForm.StatusLabel.Caption := StageOpenDssInstallation;
  end;

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

  if CurStep = ssPostInstall then
  begin
    WizardForm.PageNameLabel.Caption := StageTrainingEnvironmentSetup;
    WizardForm.StatusLabel.Caption :=
      StageTrainingEnvironmentSetup + ': downloading and verifying...';
    RunTrainingProvisioner;
  end;
end;

function NeedRestart: Boolean;
begin
  Result := VcRestartRequired;
end;

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
