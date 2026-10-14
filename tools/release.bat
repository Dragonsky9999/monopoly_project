@echo off
rem Build everything, make the installer, and (re)install it for the current user.
rem Logs: tools\build_gui_console.txt, tools\install_app_console.txt
call "%~dp0build_gui.bat"
findstr /c:"BUILD_OK" "%~dp0build_gui_console.txt" >nul || exit /b 1
call "%~dp0install_app.bat"
