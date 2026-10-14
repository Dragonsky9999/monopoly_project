@echo off
copy /y "%APPDATA%\MonopolyRecorder\MonopolyRecorder\autosave.log" "%~dp0autosave_copy.log" > "%~dp0copy_autosave_console.txt" 2>&1
