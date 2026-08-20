call tools\EmbedPDB\EmbedPDB.exe bin\Release\SniperGhostWarrior.FusionFix.asi

powershell -NoProfile -ExecutionPolicy Bypass -File "sign.ps1" -SearchPaths ".\bin\Release\SniperGhostWarrior.FusionFix.asi"

copy bin\Release\SniperGhostWarrior.FusionFix.asi data\plugins\SniperGhostWarrior.FusionFix.asi

7z a "SniperGhostWarrior.FusionFix.zip" ".\data\*" ^
-xr!*\.gitkeep
