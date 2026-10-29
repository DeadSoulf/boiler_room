@echo off
setlocal
chcp 65001 >nul
set PYTHONUTF8=1
set "BOILER_PACKAGE=%~dp0.."
if defined BOILER_FLASH_PYTHON goto selected_python
if exist "%BOILER_PACKAGE%\.venv\Scripts\python.exe" goto venv
where py >nul 2>nul
if not errorlevel 1 goto launcher
where python >nul 2>nul
if not errorlevel 1 goto python
echo Требуется Python 3.10 или новее. См. docs\flash-panel.md.
set "BOILER_RESULT=1"
goto done
:selected_python
if not exist "%BOILER_FLASH_PYTHON%" (
  echo Не найден выбранный Python. Проверьте BOILER_FLASH_PYTHON.
  set "BOILER_RESULT=1"
  goto done
)
"%BOILER_FLASH_PYTHON%" "%~dp0flash_panel.py" --package "%BOILER_PACKAGE%" --port COM3 --write
set "BOILER_RESULT=%ERRORLEVEL%"
goto done
:venv
"%BOILER_PACKAGE%\.venv\Scripts\python.exe" "%~dp0flash_panel.py" --package "%BOILER_PACKAGE%" --port COM3 --write
set "BOILER_RESULT=%ERRORLEVEL%"
goto done
:launcher
py -3 "%~dp0flash_panel.py" --package "%BOILER_PACKAGE%" --port COM3 --write
set "BOILER_RESULT=%ERRORLEVEL%"
goto done
:python
python "%~dp0flash_panel.py" --package "%BOILER_PACKAGE%" --port COM3 --write
set "BOILER_RESULT=%ERRORLEVEL%"
:done
echo.
if "%BOILER_RESULT%"=="0" (echo Готово. Проверьте экран и версию в разделе Сервис.) else (echo Загрузка не завершена. Сохраните сообщение об ошибке.)
pause
exit /b %BOILER_RESULT%
