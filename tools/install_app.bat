@echo off
rem Installs the built Setup.exe silently for the current user (no admin).
rem An old install is removed first, and we WAIT until the old uninstaller has finished
rem (it relaunches itself in the background; installing too early lets it delete the new files).
rem Log: tools\install_app_console.txt
set TARGET=%LOCALAPPDATA%\Programs\BusinessMonopolyRecorder
set LOG=%~dp0install_app_console.txt
set SETUP=
for %%f in ("%~dp0..\dist\BusinessMonopolyRecorder-*-Setup.exe") do set SETUP=%%~ff
echo setup=%SETUP% > "%LOG%"
if "%SETUP%"=="" exit /b 1
tasklist /FI "IMAGENAME eq monopoly_gui*" | find /I "monopoly_gui" >nul
if not errorlevel 1 (
  echo APP_RUNNING: close Business Monopoly Recorder and run this again >> "%LOG%"
  powershell -NoProfile -Command "Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show('Business Monopoly Recorder is running. Close it and run install_app.bat again.','install_app') | Out-Null"
  exit /b 1
)
if exist "%TARGET%\Uninstall.exe" (
  echo uninstalling old version >> "%LOG%"
  "%TARGET%\Uninstall.exe" purge --confirm-command >> "%LOG%" 2>&1
)
set /a WAITED=0
:waitloop
if not exist "%TARGET%" goto install
if %WAITED% GEQ 60 goto forceclean
timeout /t 2 /nobreak >nul
set /a WAITED+=2
goto waitloop
:forceclean
echo old folder still present after %WAITED%s, removing leftovers >> "%LOG%"
rd /s /q "%TARGET%" >> "%LOG%" 2>&1
:install
echo waited %WAITED%s for old uninstaller >> "%LOG%"
"%SETUP%" install --root "%TARGET%" --accept-licenses --default-answer --confirm-command >> "%LOG%" 2>&1
echo exit %errorlevel% >> "%LOG%"
timeout /t 5 /nobreak >nul
echo --- files in bin --- >> "%LOG%"
dir /b "%TARGET%\bin" >> "%LOG%" 2>&1
if exist "%TARGET%\bin\monopoly_gui.exe" (echo INSTALL_OK >> "%LOG%") else (echo INSTALL_BROKEN >> "%LOG%")
