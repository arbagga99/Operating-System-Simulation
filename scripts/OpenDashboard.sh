#!/usr/bin/env bash
set -e
THIS="$(cd "$(dirname "$0")" && pwd)"
cd "$THIS/.."
if command -v python >/dev/null 2>&1; then
  python -m http.server 8010 >/dev/null 2>&1 &
  sleep 1
  xdg-open "http://localhost:8010/dashboards/schedule_dashboard.html" >/dev/null 2>&1 || open "http://localhost:8010/dashboards/schedule_dashboard.html"
else
  xdg-open "dashboards/schedule_dashboard.html" >/dev/null 2>&1 || open "dashboards/schedule_dashboard.html"
fi
