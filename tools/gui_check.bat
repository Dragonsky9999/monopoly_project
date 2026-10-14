@echo off
rem Rebuild GUI (no installer) and save screenshots of the demo log into tools\shots\
set ROOT=%~dp0..
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_gui.ps1" -NoInstaller > "%~dp0gui_check_console.txt" 2>&1
if not exist "%~dp0shots" mkdir "%~dp0shots"
del /q "%~dp0shots\*.png" 2>nul
set EXE=%ROOT%\dist\app\bin\monopoly_gui.exe
"%EXE%" --open "%~dp0demo_game.log" --goto 7 --screenshot "%~dp0shots\1_purchase.png"
"%EXE%" --open "%~dp0demo_game.log" --goto 33 --demo-tray --screenshot "%~dp0shots\2_negotiation.png"
"%EXE%" --open "%~dp0demo_game.log" --hover P-BR1 --screenshot "%~dp0shots\3_latest.png"
"%EXE%" --open "%~dp0demo_game.log" --goto 7 --hover P-OR1 --text-scale 1.45 --screenshot "%~dp0shots\4_purchase_big.png"
dir /b "%~dp0shots" >> "%~dp0gui_check_console.txt"
echo GUI_CHECK_DONE >> "%~dp0gui_check_console.txt"
