@echo off
setlocal
cd /d "%~dp0"
if not exist "bin\Release\SniperGhostWarrior.FusionFix.asi" exit /b 1
"tools\EmbedPDB\EmbedPDB.exe" "bin\Release\SniperGhostWarrior.FusionFix.asi"
if errorlevel 1 exit /b %errorlevel%
if defined CODE_SIGNING_PFX if defined CODE_SIGNING_PASSWORD (
    powershell -NoProfile -ExecutionPolicy Bypass -File "sign.ps1" -SearchPathsRaw "%CD%\bin\Release\SniperGhostWarrior.FusionFix.asi"
    if errorlevel 1 exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "package.ps1" %*
exit /b %errorlevel%
