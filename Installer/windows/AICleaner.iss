; AI Cleaner - Windows installer (Inno Setup)
#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

[Setup]
AppId={{6E2F3C1A-9B57-4C2E-A8D1-3F0B7A5C9E21}
AppName=AI Cleaner
AppVersion={#MyAppVersion}
AppPublisher=MAAURA and DANIDANE
AppPublisherURL=https://www.instagram.com/maaura.music/
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=AI-Cleaner-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName=AI Cleaner
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Files]
Source: "..\..\build\AICleaner_artefacts\Release\VST3\AI Cleaner.vst3\*"; DestDir: "{commoncf64}\VST3\AI Cleaner.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
english.FinishedLabel=AI Cleaner is installed. Open your music software and look for "AI Cleaner" (by MAAURA) in your effects. In FL Studio, use Options > Manage plugins > Find plugins if it does not show up yet.
french.FinishedLabel=AI Cleaner est installé. Ouvre ton logiciel de musique et cherche « AI Cleaner » (MAAURA) dans tes effets. Dans FL Studio, fais Options > Manage plugins > Find plugins s'il n'apparaît pas encore.
