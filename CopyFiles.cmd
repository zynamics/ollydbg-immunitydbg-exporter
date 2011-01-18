@echo off
setlocal enabledelayedexpansion

if not "%SKIP_POST_BUILD_EVENTS%" == "" (
	goto :eof
)

copy r:\sabre\OllyDbgExporter\InitializeTables.sql r:\sabre\odbg110 /Y
copy r:\sabre\OllyDbgExporter\ZynamicsExporter.xml r:\sabre\odbg110 /Y
copy r:\sabre\OllyDbgExporter\bin\OllyDbg\ZynamicsExporter.dll r:\sabre\odbg110 /Y

endlocal
