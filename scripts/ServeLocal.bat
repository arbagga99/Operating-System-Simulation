@echo off
pushd %~dp0\..
start "" python -m http.server 8010
timeout /t 1 >nul
start "" http://localhost:8010/dashboards/schedule_dashboard.html
popd
