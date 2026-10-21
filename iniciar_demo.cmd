@echo off
setlocal
cd /d "%~dp0"
if exist "build\CMakeCache.txt" (
    cmake -S . -B build
) else (
    where ninja >nul 2>nul
    if errorlevel 1 (
        cmake -S . -B build
    ) else (
        cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    )
)
if errorlevel 1 goto fail
cmake --build build --config Release
if errorlevel 1 goto fail
if exist ".venv\Scripts\python.exe" (
    ".venv\Scripts\python.exe" scripts\demo.py %*
    goto done
)
if exist "%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" (
    "%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" scripts\demo.py %*
    goto done
)
where py >nul 2>nul
if not errorlevel 1 (
    py -3 scripts\demo.py %*
    goto done
)
python scripts\demo.py %*
:done
if errorlevel 1 goto fail
exit /b 0
:fail
echo.
echo No se pudo iniciar la demo. Revisa el error anterior y las instrucciones en README.md.
pause
exit /b 1
