@echo off
setlocal
set CS=%1
if "%CS%"=="" set CS=1
set Q=%2
if "%Q%"=="" set Q=3

make
python scripts\CompareAlgorithms.py %CS% %Q%
echo Done. See out\summary.csv and out\report.md
echo Use dashboards\schedule_dashboard.html to visualize any out\<algo>\*.csv
