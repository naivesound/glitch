@echo off

reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && set UNAME=i686 || set UNAME=amd64
set PATH=%PATH%;C:\Program Files\7-Zip;C:\Program Files\Git\bin

set VERSION=0.0.0

set DIR=%0\..\..
set DISTNAME=glitch-windows-%UNAME%-%VERSION%
set DISTDIR=%DIR%\dist\%DISTNAME%

mkdir %DISTDIR%

windres -i %DIR%\dist\glitch.rc -O coff -o %DIR%\cmd\glitch\glitch_rc.syso

go get -u github.com/jteeuwen/go-bindata/...
go get github.com/naivesound/glitch/cmd/glitch
go generate github.com/naivesound/glitch/cmd/glitch
go test github.com/naivesound/glitch/...
go build -ldflags "-H windowsgui" -o %DISTDIR%\glitch.exe github.com/naivesound/glitch/cmd/glitch

xcopy %DIR%\API.md %DISTDIR%
xcopy %DIR%\samples %DISTDIR%

cd %DIR%/dist

7z a %DISTNAME%.zip %DISTNAME%
pause
