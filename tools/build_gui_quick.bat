@echo off
rem Build GUI only, no installer (log: tools\build_gui.log)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_gui.ps1" -NoInstaller
