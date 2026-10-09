; Windows installer for an Apex plug-in (Inno Setup 6). CI builds it with:
;   iscc /DProduct="ApexAmp" /DSlug=ApexAmp /DAppGuid=... /DVersion=0.11.0
;        /DPublisher="..." /DSourceDir=staging /DOutputDir=out apex.iss
; SourceDir holds VST3\<Product>.vst3, Standalone\<Product>.exe, EULA.txt and
; THIRD_PARTY.txt.

#ifndef Product
  #error Define Product, Slug, AppGuid, Version, Publisher, SourceDir and OutputDir
#endif

[Setup]
AppId={{{#AppGuid}}
AppName={#Product}
AppVersion={#Version}
AppVerName={#Product} {#Version}
AppPublisher={#Publisher}
DefaultDirName={autopf}\Apex\{#Product}
DefaultGroupName=Apex
DisableProgramGroupPage=yes
DisableDirPage=auto
LicenseFile={#SourceDir}\EULA.txt
OutputDir={#OutputDir}
OutputBaseFilename={#Slug}-{#Version}-Windows-Setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#Product}
UninstallDisplayIcon={app}\{#Product}.exe

[Types]
Name: "full"; Description: "VST3 plug-in and standalone app"
Name: "custom"; Description: "Choose what to install"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (for your DAW)"; Types: full custom
Name: "standalone"; Description: "Standalone app (play without a DAW)"; Types: full

[Files]
Source: "{#SourceDir}\VST3\{#Product}.vst3\*"; DestDir: "{commoncf64}\VST3\{#Product}.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Standalone\{#Product}.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#SourceDir}\EULA.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\THIRD_PARTY.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#Product}"; Filename: "{app}\{#Product}.exe"; Components: standalone
Name: "{group}\Uninstall {#Product}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#Product}.exe"; Description: "Start {#Product} now"; Flags: nowait postinstall skipifsilent unchecked; Components: standalone
