@echo off
mkdir bin 2>nul
make
if %errorlevel% neq 0 exit /b %errorlevel%
mkdir out 2>nul
bin\os_sim -i data\processes.csv -a rr -q 3 -cs 1 -o out
echo Timeline -> out\timeline.csv
echo Metrics  -> out\metrics.csv
