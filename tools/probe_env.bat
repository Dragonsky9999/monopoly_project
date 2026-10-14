@echo off
rem Environment check only (installs nothing). Output: tools\probe.log
chcp 65001 >nul
set LOG=%~dp0probe.log
echo === probe %date% %time% === > "%LOG%"
echo [python] >> "%LOG%"
where python >> "%LOG%" 2>&1
python --version >> "%LOG%" 2>&1
where py >> "%LOG%" 2>&1
py --version >> "%LOG%" 2>&1
echo [cmake] >> "%LOG%"
where cmake >> "%LOG%" 2>&1
cmake --version >> "%LOG%" 2>&1
echo [vswhere] >> "%LOG%"
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -all -products * -format text -property installationPath >> "%LOG%" 2>&1
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -all -products * -format text -property displayName >> "%LOG%" 2>&1
echo [vs cmake] >> "%LOG%"
dir /b "%ProgramFiles%\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin" >> "%LOG%" 2>&1
echo [msvc tools] >> "%LOG%"
dir /b "%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Tools\MSVC" >> "%LOG%" 2>&1
echo [Qt dir] >> "%LOG%"
dir /b C:\Qt >> "%LOG%" 2>&1
echo [disk] >> "%LOG%"
powershell -NoProfile -Command "Get-PSDrive C | Select-Object Used,Free | Format-List" >> "%LOG%" 2>&1
echo [winget] >> "%LOG%"
where winget >> "%LOG%" 2>&1
echo [net] >> "%LOG%"
powershell -NoProfile -Command "try{(Invoke-WebRequest -UseBasicParsing -Uri https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/ -TimeoutSec 15).StatusCode}catch{$_.Exception.Message}" >> "%LOG%" 2>&1
powershell -NoProfile -Command "try{(Invoke-WebRequest -UseBasicParsing -Uri https://pypi.org/simple/aqtinstall/ -TimeoutSec 15).StatusCode}catch{$_.Exception.Message}" >> "%LOG%" 2>&1
echo === done === >> "%LOG%"
