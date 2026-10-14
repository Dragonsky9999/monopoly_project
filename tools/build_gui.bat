@echo off
rem Build GUI + installer (log: tools\build_gui.log, console: tools\build_gui_console.txt)
echo started %date% %time% > "%~dp0build_gui_console.txt"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_gui.ps1" %* >> "%~dp0build_gui_console.txt" 2>&1
echo exit %errorlevel% >> "%~dp0build_gui_console.txt"
