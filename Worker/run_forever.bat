@echo off
:loop
echo Starting worker...
node worker.js
timeout 5
goto loop