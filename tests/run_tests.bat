@echo off
setlocal
set CS=%1
if "%CS%"=="" set CS=1

set ROOT=%~dp0..
make

for %%A in (fcfs rr sjf srtf prio) do (
  set OUT=%ROOT%\out\%%A
  if not exist "%OUT%" mkdir "%OUT%"
  if "%%A"=="rr" (
    bin\os_sim -i data\processes.csv -a rr -q 3 -cs %CS% -o "%OUT%"
  ) else (
    bin\os_sim -i data\processes.csv -a %%A -cs %CS% -o "%OUT%"
  )
  python tests\validator.py data\processes.csv "%OUT%\timeline.csv" "%OUT%\metrics.csv" %CS% >nul
  if errorlevel 1 exit /b 1
  echo [OK] %%A
)

echo All tests passed.
