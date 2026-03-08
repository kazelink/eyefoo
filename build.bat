@echo off

echo [*] Compiling Resources...
windres res\resource.rc res\resource.o
if %errorlevel% neq 0 (
    echo Resource Compile FAILED.
    pause
    exit /b 1
)

echo [*] Compiling C source files...
gcc src\main.c src\logic.c src\hud.c src\tray.c src\config_ui.c src\utils.c res\resource.o -o eye_reminder.exe -Iinclude -DUNICODE -D_UNICODE -mwindows -Os -s -lshell32 -lcomctl32 -lgdi32 -ldwmapi -lwtsapi32

if %errorlevel% neq 0 (
    echo Compilation FAILED.
    pause
    exit /b 1
)
del res\resource.o
echo [*] DONE. Success!
pause