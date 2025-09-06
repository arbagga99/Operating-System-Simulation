@echo off
REM Option A: serve folder with Python if available (avoids file:// CORS)
where python >nul 2>nul
if %errorlevel%==0 (
  pushd %~dp0\..
  start "" python -m http.server 8010
  timeout /t 1 >nul
  start "" http://localhost:8010/dashboards/schedule_dashboard.html
  popd
) else (
  REM Fallback: open directly (works if you upload CSVs via the page)
  start "" "%~dp0\..\dashboards\schedule_dashboard.html"
)
