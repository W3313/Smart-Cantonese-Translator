; Inno Setup 6 script for Smart Cantonese Translator.
;
; Packs the staged install tree produced by
;     cmake --install <build> --prefix <stage>
; into a single per-user setup .exe (no admin rights needed; an "install for
; all users" choice is offered in a dialog).
;
; Build (CI does this in .github/workflows/windows.yml):
;     iscc /DAppVersion=1.2.3 /DStageDir=C:\path\to\stage /DOutputDir=C:\path\to\dist packaging\windows\installer.iss
;
; AppVersion must be numeric (major.minor.patch); it is taken from the CMake project version.
; Pass absolute paths for StageDir/OutputDir. Other relative paths below are relative to
; this script's folder.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
; Defaults match docs/BUILDING.md (cmake --install build --prefix dist\SmartCantoneseTranslator).
#ifndef StageDir
  #define StageDir AddBackslash(SourcePath) + "..\..\dist\SmartCantoneseTranslator"
#endif
#ifndef OutputDir
  #define OutputDir AddBackslash(SourcePath) + "..\..\dist"
#endif

#define AppName       "Smart Cantonese Translator"
#define AppExeName    "SmartCantoneseTranslator.exe"
#define AppPublisher  "Smart Cantonese Translator contributors"
#define AppURL        "https://github.com/w3313/smart-cantonese-translator"

#if !FileExists(AddBackslash(StageDir) + AppExeName)
  #error StageDir does not contain SmartCantoneseTranslator.exe. Run cmake --install first and pass /DStageDir=<that folder>.
#endif

[Setup]
; Fixed GUID: never change it, or upgrades will install side by side instead of in place.
AppId={{5C7E4327-C922-4DB4-B2DB-87210D062CEC}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
AppCopyright=Copyright (C) {#AppPublisher}
VersionInfoVersion={#AppVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} Setup
VersionInfoProductTextVersion={#AppVersion}

; Per-user install by default (%LOCALAPPDATA%\Programs\...), with an optional
; "install for all users" (Program Files, needs admin) choice.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DefaultDirName={autopf}\{#AppName}
DisableProgramGroupPage=yes
UsePreviousAppDir=yes

; 64-bit only; Qt 6.8 supports Windows 10 1809 and later.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763

OutputDir={#OutputDir}
OutputBaseFilename=SmartCantoneseTranslator-Setup-{#AppVersion}
SetupIconFile=..\..\resources\icons\app.ico
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
; Close a running copy of the app (Restart Manager) before replacing its files.
CloseApplications=yes
RestartApplications=no
ShowLanguageDialog=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
; Remove plugins/translations from a previous version so stale Qt plugin DLLs
; (built against another Qt release) are never loaded after an upgrade.
Type: filesandordirs; Name: "{app}\plugins"
Type: filesandordirs; Name: "{app}\translations"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent runasoriginaluser
