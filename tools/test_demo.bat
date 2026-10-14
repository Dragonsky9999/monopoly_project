@echo off
rem Backs up the app autosave, then opens the demo game in the installed app (for testing)
set AS=%APPDATA%\MonopolyRecorder\MonopolyRecorder\autosave.log
if exist "%AS%" copy /y "%AS%" "%~dp0autosave_backup.log" >nul
start "" "%LOCALAPPDATA%\Programs\BusinessMonopolyRecorder\bin\monopoly_gui.exe" --open "%~dp0demo_game.log"
