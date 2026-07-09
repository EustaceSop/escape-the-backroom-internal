@echo off
setlocal

:: Project folders
set SRC_PATH=src
set OBJ_PATH=obj
set BIN_PATH=bin

:: Path to the Dumper-7 generated SDK
set SDK_PATH=C:\Dumper-7\4.27.2-0+++UE4+Release-4.27-EscapeTheBackrooms\CppSDK

:: Path to the rendezvous GUI framework (now vendored inside the project)
set RV_PATH=third_party\rendezvous

:: Path to stb headers (for rendezvous font/image decoding)
set STB_PATH=third_party\stb

:: Visual Studio environment (adjust to your installation)
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" 2>nul
)
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" 2>nul
)

if not exist "%OBJ_PATH%" mkdir "%OBJ_PATH%"
if not exist "%BIN_PATH%" mkdir "%BIN_PATH%"

echo Compiling EscapeTheBackrooms internal with rendezvous menu...

cl.exe /EHsc /std:c++latest /O2 /MD /W3 /nologo ^
    /I"%SDK_PATH%" ^
    /I"%RV_PATH%" ^
    /I"%STB_PATH%" ^
    /I"%SRC_PATH%" ^
    /D NDEBUG ^
    /utf-8 /Zc:preprocessor ^
    /Fo"%OBJ_PATH%/" ^
    "%SRC_PATH%\dllmain.cpp" ^
    "%SRC_PATH%\etb_menu.cpp" ^
    "%SRC_PATH%\hotkeys.cpp" ^
    "%RV_PATH%\gui\comp\text_edit.cpp" ^
    "%RV_PATH%\gui\comp\checkbox.cpp" ^
    "%RV_PATH%\gui\comp\color_picker.cpp" ^
    "%RV_PATH%\gui\comp\combo_box.cpp" ^
    "%RV_PATH%\gui\comp\multi_combo_box.cpp" ^
    "%RV_PATH%\gui\comp\key_bind.cpp" ^
    "%RV_PATH%\gui\comp\panel.cpp" ^
    "%RV_PATH%\gui\comp\plot_lines.cpp" ^
    "%RV_PATH%\gui\comp\text.cpp" ^
    "%RV_PATH%\gui\comp\text_box.cpp" ^
    "%RV_PATH%\gui\comp\tabs.cpp" ^
    "%RV_PATH%\gui\comp\progress_bar.cpp" ^
    "%RV_PATH%\gui\comp\radio_button.cpp" ^
    "%RV_PATH%\gui\comp\collapsing_header.cpp" ^
    "%RV_PATH%\gui\comp\plot_histogram.cpp" ^
    "%RV_PATH%\gui\elements.cpp" ^
    "%RV_PATH%\gui\layout.cpp" ^
    "%RV_PATH%\gui\layout_position.cpp" ^
    "%RV_PATH%\input\win32.cpp" ^
    "%RV_PATH%\render\impl\dx11.cpp" ^
    "%RV_PATH%\render\render.cpp" ^
    "%RV_PATH%\render\render_shapes.cpp" ^
    "%RV_PATH%\render\render_text.cpp" ^
    "%RV_PATH%\render\render_path.cpp" ^
    "%RV_PATH%\render\image_load.cpp" ^
    "%SDK_PATH%\SDK\Basic.cpp" ^
    "%SDK_PATH%\SDK\CoreUObject_functions.cpp" ^
    "%SDK_PATH%\SDK\Engine_functions.cpp" ^
    "%SDK_PATH%\SDK\Backrooms_functions.cpp" ^
    "%SDK_PATH%\SDK\BPCharacter_Demo_functions.cpp" ^
    "%SDK_PATH%\SDK\BP_MyGameInstance_functions.cpp" ^
    "%SDK_PATH%\SDK\MP_GameMode_functions.cpp" ^
    "%SDK_PATH%\SDK\MP_PlayerController_functions.cpp" ^
    /link /DLL /OUT:"%BIN_PATH%\EscapeInternal.dll" user32.lib gdi32.lib gdiplus.lib d3d11.lib

if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

:: Also copy a build to the root for injectors that still expect the old flat layout.
copy /Y "%BIN_PATH%\EscapeInternal.dll" "EscapeInternal.dll" > nul 2>&1

echo Build complete: %BIN_PATH%\EscapeInternal.dll
endlocal
