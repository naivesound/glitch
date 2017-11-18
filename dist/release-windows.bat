@echo off

reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && set UNAME=i686 || set UNAME=amd64
set PATH=%PATH%;C:\Program Files\7-Zip;C:\Program Files\Git\bin

FOR /F "tokens=1 delims=" %%A in ('git describe --abbrev^=0 --tags') do SET VERSION=%%A

set DIR=%0\..\..
set DISTNAME=glitch-windows-%UNAME%-%VERSION%
set DISTDIR=%DIR%\dist\%DISTNAME%

mkdir %DISTDIR%

windres -i %DIR%\dist\glitch.rc -O coff -o %DIR%\cmd\glitch\glitch_rc.syso

go generate github.com/naivesound/glitch/cmd/glitch
go test github.com/naivesound/glitch/...
go vet github.com/naivesound/glitch/...
go build -ldflags "-H windowsgui" -o %DISTDIR%\glitch.exe github.com/naivesound/glitch/cmd/glitch

copy /y %DIR%\LICENSE %DISTDIR%\LICENSE.txt
xcopy /y %DIR%\API.md %DISTDIR%
xcopy /i /y %DIR%\examples %DISTDIR%\examples
xcopy /i /y %DIR%\samples %DISTDIR%\samples

cd %DIR%/dist

7z a %DISTNAME%.zip %DISTNAME%

