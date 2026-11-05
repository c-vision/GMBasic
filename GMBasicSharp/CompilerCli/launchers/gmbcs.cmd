@echo off
rem gmbcs - launcher Windows, copiato dalla build in <repo>\bin\gmbcs.cmd.
rem L'app .NET (assembly + include/*.inc + system.settings.json) sta in gmbcs-app\.
rem %~dp0 = cartella di questo file (con la barra finale): nessuna dipendenza dalla cwd.
"%~dp0gmbcs-app\gmbcs.exe" %*
