@echo off
setlocal enabledelayedexpansion

if not "%SKIP_POST_BUILD_EVENTS%" == "" (
	goto :eof
)

copy r:\sabre\OllyDbgExporter\InitializeTables.sql "C:\Programme\Immunity Debugger" /Y
copy r:\sabre\OllyDbgExporter\ZynamicsExporter.xml "C:\Programme\Immunity Debugger" /Y
copy r:\sabre\OllyDbgExporter\bin\Immunity\ZynamicsExporter.dll "C:\Programme\Immunity Debugger" /Y

endlocal
