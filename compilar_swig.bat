@echo off
echo =======================================================
echo   COMPILACION CON SWIG - PROYECTO PARQUEADERO
echo =======================================================
echo.

:: 1. Verificar si swig.exe esta disponible
where swig.exe >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] No se encontro 'swig.exe' en el PATH.
    echo.
    echo Para compilar con SWIG, por favor:
    echo   1. Descargue SWIG para Windows desde: http://www.swig.org/download.html
    echo   2. Extraiga el ZIP (ej. swigwin-X.Y.Z) y agregue la carpeta al PATH
    echo      o copie 'swig.exe' (junto con su archivo 'swig.swg' y demas archivos) a esta carpeta.
    echo.
    echo NOTA: El visualizador seguira funcionando usando ctypes como fallback si no compila con SWIG.
    echo.
    pause
    exit /b 1
)

if not exist build mkdir build

:: 2. Ejecutar SWIG
echo [1/3] Generando wrappers de Python con SWIG...
swig -python -c++ -o src\parqueadero_lib_swig_wrap.cpp src\parqueadero_lib_swig.i
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Fallo la generacion de wrappers con SWIG.
    pause
    exit /b 1
)
echo    OK

:: 3. Obtener ruta de instalacion de Python dinamicamente
echo [2/3] Buscando directorio de desarrollo de Python...
for /f "delims=" %%i in ('py -c "import sys; print(sys.prefix)"') do set PYTHON_DIR=%%i
echo    Directorio encontrado: %PYTHON_DIR%

:: 4. Compilar modulo de SWIG (_parqueadero_lib_swig.pyd)
echo [3/3] Compilando libreria de SWIG (_parqueadero_lib_swig.pyd)...
g++ -shared -o src\_parqueadero_lib_swig.pyd src\parqueadero_lib.cpp src\parqueadero_lib_swig_wrap.cpp -I"%PYTHON_DIR%\include" -L"%PYTHON_DIR%\libs" -lpython3 -lws2_32 -static -DBUILDING_DLL
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Fallo la compilacion del modulo de SWIG.
    pause
    exit /b 1
)
echo    OK

:: Copiar modulo compilado al directorio build por organizacion
copy src\_parqueadero_lib_swig.pyd build\_parqueadero_lib_swig.pyd >nul
copy src\parqueadero_lib_swig.py build\parqueadero_lib_swig.py >nul

echo.
echo =======================================================
echo   COMPILACION SWIG EXITOSA
echo =======================================================
echo Modulo generado en:
echo   src\_parqueadero_lib_swig.pyd
echo   src\parqueadero_lib_swig.py
echo.
echo El visualizador detectara y cargara SWIG automaticamente al iniciar.
echo.
pause
