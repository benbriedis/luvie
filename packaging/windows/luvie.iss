; SPDX-FileCopyrightText: Ben Briedis
; SPDX-License-Identifier: Apache-2.0
;
; Inno Setup script for the Windows installer: Luvie.exe plus the luvie.lv2 plug-in bundle.
;
; Driven by tools/make-windows-installer.sh, which stages the two install components and
; passes the values below in. Build it by hand with:
;
;     iscc /DAppVersion=v0.0.5 /DStageDir=..\..\build-dist\win-stage \
;          /DOutDir=..\..\build-dist packaging\windows\luvie.iss
;
; This is the whole of the Windows distribution -- there is no .zip beside it any more
; (dropped after v0.0.6, see cmake/Packaging.cmake). One artifact is what makes signing
; meaningful: an unsigned second download beside a signed one is the one people would
; reach for when the signed one gives them any trouble. It also earns its place on its
; own, by putting luvie.lv2 where hosts look, adding a Start menu entry, registering .luv,
; and being able to uninstall itself.

#ifndef AppVersion
  #error AppVersion must be passed in with /DAppVersion=
#endif
#ifndef StageDir
  #error StageDir must be passed in with /DStageDir=
#endif
#ifndef NumericVersion
  #error NumericVersion must be passed in with /DNumericVersion=
#endif
#ifndef OutDir
  #define OutDir "."
#endif

[Setup]
; Generated once and never to be changed: this is the identity Windows matches a new
; version against, and altering it would install a second Luvie beside the first rather
; than upgrading it.
AppId={{A4F065E0-AF1A-4C33-8BEB-60E375A01730}
AppName=Luvie
AppVersion={#AppVersion}
AppVerName=Luvie {#AppVersion}
VersionInfoVersion={#NumericVersion}
AppPublisher=Ben Briedis
AppPublisherURL=https://github.com/benbriedis/luvie
AppSupportURL=https://github.com/benbriedis/luvie/issues
AppUpdatesURL=https://github.com/benbriedis/luvie/releases

; Install per-user, without elevation. This is the point of the whole file: with
; PrivilegesRequired=lowest there is no UAC prompt, and so no "Unknown publisher" shield
; on an unsigned build -- only SmartScreen, which no installer design can avoid. It also
; means the LV2 bundle and the .luv association land in the per-user locations that need no
; administrator, matching where a developer build would have put them.
PrivilegesRequired=lowest
DefaultDirName={autopf}\Luvie
DefaultGroupName=Luvie
DisableProgramGroupPage=yes
; Shown as a page in the wizard, and read from the staged tree rather than the source tree
; so the installer displays the same copy it installs.
LicenseFile={#StageDir}\share\doc\luvie\LICENSES\LICENSE
UninstallDisplayIcon={app}\luvie.exe
UninstallDisplayName=Luvie {#AppVersion}

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0

WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
OutputDir={#OutDir}
OutputBaseFilename=luvie-{#AppVersion}-windows-x86_64-setup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Standalone application and LV2 plug-in"
Name: "custom"; Description: "Choose what to install"; Flags: iscustom

[Components]
; Mirrors the CMake install components of the same names (src/CMakeLists.txt and
; src/lv2/CMakeLists.txt), so the choice offered here is the one the build already makes.
; Someone sequencing hardware synths has no use for the plug-in, and someone who works
; entirely inside a DAW has no use for the application.
Name: "standalone"; Description: "Luvie application"; Types: full custom
Name: "plugin";     Description: "LV2 plug-in (for Reaper, Ardour, Carla, ...)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: standalone; Flags: unchecked
Name: "associate";   Description: "Open .luv project files with Luvie"; Components: standalone

[Files]
Source: "{#StageDir}\bin\luvie.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
; Apache-2.0 sections 4a/4d: the licence and NOTICE travel with any redistributed copy,
; alongside those of the libraries compiled in. Installed with the application, since that
; is the half most people install; the plug-in bundle carries its own copy internally.
Source: "{#StageDir}\share\doc\luvie\LICENSES\*"; DestDir: "{app}\LICENSES"; Components: standalone; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\README.txt"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

; %APPDATA%\LV2 is the per-user bundle directory lilv searches on Windows, and is already
; the build's own default (LV2_INSTALL_DIR in src/lv2/CMakeLists.txt) -- so an installed
; plug-in ends up exactly where a developer build would have put it.
Source: "{#StageDir}\lib\lv2\luvie.lv2\*"; DestDir: "{userappdata}\LV2\luvie.lv2"; Components: plugin; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Luvie";           Filename: "{app}\luvie.exe"; Components: standalone
Name: "{autodesktop}\Luvie";     Filename: "{app}\luvie.exe"; Components: standalone; Tasks: desktopicon

[Registry]
; Under HKCU, not HKCR: this installer never elevates, and a per-user association is all
; that is wanted anyway. luvie.exe already takes a project path as its first argument
; (see src/main.cpp), so the association needs nothing on the application side.
Root: HKCU; Subkey: "Software\Classes\.luv"; ValueType: string; ValueName: ""; ValueData: "Luvie.Project"; Flags: uninsdeletevalue; Tasks: associate
Root: HKCU; Subkey: "Software\Classes\Luvie.Project"; ValueType: string; ValueName: ""; ValueData: "Luvie Project"; Flags: uninsdeletekey; Tasks: associate
Root: HKCU; Subkey: "Software\Classes\Luvie.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\luvie.exe,0"; Tasks: associate
Root: HKCU; Subkey: "Software\Classes\Luvie.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\luvie.exe"" ""%1"""; Tasks: associate

[Run]
Filename: "{app}\luvie.exe"; Description: "{cm:LaunchProgram,Luvie}"; Components: standalone; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The bundle directory itself, once its files have gone: [Files] entries remove what they
; installed but leave the directories they created.
Type: dirifempty; Name: "{userappdata}\LV2\luvie.lv2"
Type: dirifempty; Name: "{app}\LICENSES"
