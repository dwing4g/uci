@echo off
for %%x in (%*) do cd /d "%%~dpx" & imgdec "%%~nxx" - | ucienc - -o "%%~nx.uci"
pause
