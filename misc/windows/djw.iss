; SPDX-License-Identifier: AGPL-3.0-or-later
; Copyright (C) 2026 Niko Sagiadinos

; Inno Setup script for the Windows installer of DJ-Workstation.
; Called by the build script with:
;   ISCC.exe /DAppVersion=1.0.0-dev.123 /DAppVersionNumbers=1.0.0 /DFilesDir=<files to install> /DOutputDir=<dir> djw.iss
;
; Updates: AppId must never change. An update installs into the same folder, closes a running djw.exe and
; overwrites the files; DLLs, fonts and mappings of the old version are removed first. The user's settings and own
; mappings live in %APPDATA%\djw and are not touched.

#ifndef AppVersion
  #error AppVersion is not defined
#endif
#ifndef AppVersionNumbers
  #error AppVersionNumbers is not defined
#endif
#ifndef FilesDir
  #error FilesDir is not defined
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{2FF07625-80AA-4578-86DA-CA388834F6E3}
AppName=DJ-Workstation
AppVersion={#AppVersion}
AppVerName=DJ-Workstation {#AppVersion}
AppPublisher=Niko Sagiadinos
AppPublisherURL=https://dj-workstation.com
AppSupportURL=https://github.com/sagiadinos/dj-workstation
AppUpdatesURL=https://dj-workstation.com
VersionInfoVersion={#AppVersionNumbers}
DefaultDirName={autopf}\DJ-Workstation
DefaultGroupName=DJ-Workstation
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=force
RestartApplications=no
LicenseFile=..\..\LICENSE
SetupIconFile=..\icons\windows\djw.ico
UninstallDisplayIcon={app}\djw.exe
UninstallDisplayName=DJ-Workstation
OutputDir={#OutputDir}
OutputBaseFilename=DJ-Workstation-{#AppVersion}Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
; Leftovers of the previous version, so nothing old stays next to the new djw.exe
Type: files; Name: "{app}\*.dll"
Type: filesandordirs; Name: "{app}\fonts"
Type: filesandordirs; Name: "{app}\mappings"

[Files]
Source: "{#FilesDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\DJ-Workstation"; Filename: "{app}\djw.exe"
Name: "{autodesktop}\DJ-Workstation"; Filename: "{app}\djw.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\djw.exe"; Description: "{cm:LaunchProgram,DJ-Workstation}"; Flags: nowait postinstall skipifsilent
