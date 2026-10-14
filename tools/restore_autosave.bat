@echo off
rem Restores the autosave backed up by test_demo.bat (close the app first)
set AS=%APPDATA%\MonopolyRecorder\MonopolyRecorder\autosave.log
if exist "%~dp0autosave_backup.log" copy /y "%~dp0autosave_backup.log" "%AS%" > "%~dp0restore_console.txt"
