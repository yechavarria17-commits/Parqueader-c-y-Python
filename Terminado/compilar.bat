@echo off
echo ============================================
echo   COMPILACION - PROYECTO PARQUEADERO
echo ============================================
echo.

set SWIG_OK=0

if not exist build mkdir build

echo [1/3] Compilando libreria dinamica base (parqueadero_lib.dll)...
g++ -shared -o build\parqueadero_lib.dll src\parqueadero_lib.cpp -lws2_32 -static -DBUILDING_DLL
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: No se pudo compilar la libreria dinamica.
    pause
    exit /b 1
)
copy build\parqueadero_lib.dll src\parqueadero_lib.dll >nul
echo    OK

echo [2/3] Compilando generador de placas (generador.exe)...
g++ -o build\generador.exe src\generador.cpp -lws2_32 -static
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: No se pudo compilar el generador.
    pause
    exit /b 1
)
echo    OK

echo [3/3] Intentando compilar modulo SWIG...
where swig.exe >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo    [AVISO] No se detecto swig.exe. Se omitira la compilacion de SWIG.
    echo            El visualizador usara ctypes como fallback funcional.
    goto END_COMPILATION
)

swig -python -c++ -o src\parqueadero_lib_swig_wrap.cpp src\parqueadero_lib_swig.i
if %ERRORLEVEL% NEQ 0 (
    echo    [ERROR] Fallo al generar wrappers de SWIG.
    goto END_COMPILATION
)

for /f "delims=" %%i in ('py -c "import sys; print(sys.prefix)"') do set PYTHON_DIR=%%i

g++ -shared -o src\_parqueadero_lib_swig.pyd src\parqueadero_lib.cpp src\parqueadero_lib_swig_wrap.cpp -I"%PYTHON_DIR%\include" -L"%PYTHON_DIR%\libs" -lpython3 -lws2_32 -static -DBUILDING_DLL
if %ERRORLEVEL% NEQ 0 (
    echo    [ERROR] Fallo al compilar modulo SWIG.
    goto END_COMPILATION
)

copy src\_parqueadero_lib_swig.pyd build\_parqueadero_lib_swig.pyd >nul
copy src\parqueadero_lib_swig.py build\parqueadero_lib_swig.py >nul
set SWIG_OK=1
echo    OK (Modulo SWIG compilado exitosamente)

:END_COMPILATION
echo.
echo ============================================
echo   COMPILACION FINALIZADA
echo ============================================
echo.
echo Archivos generados en 'build/':
echo   build\parqueadero_lib.dll       (DLL para ctypes)
echo   build\generador.exe             (Generador de placas)
if %SWIG_OK%==1 (
    echo   build\_parqueadero_lib_swig.pyd  (Modulo compilado SWIG)
    echo   build\parqueadero_lib_swig.py   (Wrapper Python SWIG)
)
echo.
echo Para ejecutar:
echo   1. Abrir visualizador:  py src\visualizador.py
echo   2. Clic en "INICIAR SERVIDOR"
echo   3. Abrir generador:     build\generador.exe
echo.
pause
