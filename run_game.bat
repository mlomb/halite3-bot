@echo off
REM halite.exe --replay-directory "Build/replays/" -vvv --width 32 --height 32 --no-logs "cd Build/Debug && MyBot.exe" "cd Build/Debug && MyBot.exe"
node run.js 32
pause