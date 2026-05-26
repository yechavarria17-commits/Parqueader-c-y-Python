@echo off
echo ============================================
echo   SISTEMA DE PARQUEADERO
echo ============================================
echo.
echo Iniciando visualizador...
echo (Haz clic en "INICIAR SERVIDOR" en la ventana)
echo (Luego ejecuta build\generador.exe en otra terminal)
echo.
start "" py src\visualizador.py
echo.
echo Presiona cualquier tecla para iniciar el generador de placas...
pause >nul
echo Iniciando generador de placas...
build\generador.exe
