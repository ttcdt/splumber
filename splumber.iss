; InnoSetup file

[Setup]
AppName=Space Plumber
AppVerName=Space Plumber version 1.1.x
DefaultDirName={pf}\splumber
UsePreviousAppDir=no
DefaultGroupName=Space Plumber
UninstallDisplayIcon={app}\splumber.exe
Compression=lzma
SolidCompression=yes
; OutputDir=userdocs:Inno Setup Examples Output

[Files]
Source: "splumber.exe"; DestDir: "{app}"
Source: "README" ; DestDir: "{app}"
Source: "LICENSE" ; DestDir: "{app}"

[Icons]
Name: "{group}\Space Plumber"; Filename: "{app}\splumber.exe"
