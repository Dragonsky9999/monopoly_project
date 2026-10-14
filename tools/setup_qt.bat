@echo off
rem Installs Qt 6 + Qt Installer Framework into C:\Qt (log: tools\setup_qt.log, console: tools\setup_qt_console.txt)
echo started %date% %time% > "%~dp0setup_qt_console.txt"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_qt.ps1" >> "%~dp0setup_qt_console.txt" 2>&1
echo exit %errorlevel% >> "%~dp0setup_qt_console.txt"
