@echo off
rem Starts a tiny local web server for the game (Python or Node.js, no admin rights needed)
rem and opens it in your browser. Without either, opens index.html directly from disk.
cd /d "%~dp0"
py -3 --version >nul 2>nul
if %errorlevel%==0 ( py -3 serve.py & goto :end )
python --version >nul 2>nul
if %errorlevel%==0 ( python serve.py & goto :end )
node --version >nul 2>nul
if %errorlevel%==0 ( node serve.mjs & goto :end )
echo Neither Python nor Node.js was found. Opening index.html directly from disk instead.
start "" "%~dp0index.html"
:end
