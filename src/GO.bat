@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem === Настройки
set "REPO_OWNER=anastas93"
set "REPO_NAME=satprjct"
set "URL_MAIN=https://github.com/%REPO_OWNER%/%REPO_NAME%/archive/refs/heads/main.zip"
set "URL_MASTER=https://github.com/%REPO_OWNER%/%REPO_NAME%/archive/refs/heads/master.zip"
set "ZIP=__repo.zip"
set "EXTRACT=__tmp_extract__"

echo === Очистка папки (кроме %~nx0)...
powershell -NoProfile -Command ^
  "Get-ChildItem -Force -LiteralPath . | Where-Object { $_.Name -ne '%~nx0' } | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue"

echo === Скачивание архива (main)...
if exist "%ZIP%" del /f /q "%ZIP%" >nul 2>&1
powershell -NoProfile -Command "try { Invoke-WebRequest -UseBasicParsing -Uri '%URL_MAIN%' -OutFile '%ZIP%' -ErrorAction Stop; exit 0 } catch { exit 1 }"
if errorlevel 1 (
  echo Не удалось main.zip, пробую master.zip...
  powershell -NoProfile -Command "try { Invoke-WebRequest -UseBasicParsing -Uri '%URL_MASTER%' -OutFile '%ZIP%' -ErrorAction Stop; exit 0 } catch { exit 1 }"
  if errorlevel 1 (
    echo [ОШИБКА] Скачивание не удалось.
    exit /b 1
  )
)

echo === Распаковка...
if exist "%EXTRACT%" rd /s /q "%EXTRACT%"
mkdir "%EXTRACT%" || (echo [ОШИБКА] mkdir && exit /b 1)

powershell -NoProfile -Command "try { Expand-Archive -LiteralPath '%ZIP%' -DestinationPath '%EXTRACT%' -Force; exit 0 } catch { exit 1 }"
if errorlevel 1 (
  echo [ОШИБКА] Не удалось распаковать архив.
  exit /b 1
)

echo === Перенос содержимого прямо в текущую папку...
set "SUBDIR="
for /d %%D in ("%EXTRACT%\*") do (
  set "SUBDIR=%%~fD"
  goto moveit
)

:moveit
if not defined SUBDIR (
  echo [ОШИБКА] Внутри архива не найдена вложенная папка.
  exit /b 1
)

rem Переносим содержимое (включая скрытые), саму обёртку не трогаем.
rem На всякий случай не перетираем выполняющийся .bat, если в архиве есть файл с тем же именем.
powershell -NoProfile -Command ^
  "$src='%SUBDIR%'; Get-ChildItem -Force -LiteralPath $src | Where-Object { $_.Name -ne '%~nx0' } | Move-Item -Destination '.' -Force -ErrorAction Stop"
if errorlevel 1 (
  echo [ОШИБКА] Не удалось перенести файлы.
  exit /b 1
)

echo === Чистка временных файлов...
rd /s /q "%EXTRACT%"
del /f /q "%ZIP%" >nul 2>&1

echo Готово.
exit /b 0
