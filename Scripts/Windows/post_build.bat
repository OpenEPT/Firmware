@echo off
setlocal ENABLEDELAYEDEXPANSION

echo ----------------------------------------

REM --- Get script directory path ---
set "ProjDirPath=%~dp0"
set "DoxyOutput=%ProjDirPath%..\..\Documentation\AdFirmware\Doxygen\doxyOutput"
set "DoxyPath=%ProjDirPath%..\..\Documentation\AdFirmware\Doxygen"
set "DoxygenDir=%ProjDirPath%\Output\doxygen"
set "ZipFile=%ProjDirPath%\Output\doxygen.zip"

REM Remove trailing backslash if present
if "%ProjDirPath:~-1%"=="\" set "ProjDirPath=%ProjDirPath:~0,-1%"

echo ProjDirPath is: %ProjDirPath%
echo DoxyPath is: %DoxyPath%

REM Check and create Output directory
if not exist "%ProjDirPath%\Output" (
    mkdir "%ProjDirPath%\Output"
)

REM Copy .srec file
xcopy /y %ProjDirPath%\..\..\Source\ADFirmware\CM7\Debug\ADFirmware_CM7.srec %ProjDirPath%\Output

where doxygen.exe >nul 2>&1

if %errorlevel%==0 (
	echo Doxygen exists
	
	echo Current directory before pushd: %CD%
	
	REM Enter Doxygen directory first
	pushd %DoxyPath%
	
	echo Current directory after pushd: !CD!

	REM Run doxygen (relative paths now relative to Doxyfile)
	doxygen.exe doxygen.txt >nul 2>&1

	REM Return to original directory
	popd

	REM Remove accidental local doxyOutput if created
	if exist "doxyOutput" (
		rmdir /s /q "doxyOutput"
	)

	REM Copy from specified source

	if exist "%DoxyOutput%" (
		echo Detected Doxygen output at: %DoxyOutput%
		if exist "%ProjDirPath%\Output\doxygen" (
			rmdir /s /q "%ProjDirPath%\Output\doxygen"
		)
		xcopy /s /e /i /y "%DoxyOutput%\*" "%ProjDirPath%\Output\doxygen\" >nul
		if exist "%DoxygenDir%" (
			echo Zipping Doxygen folder...
			powershell -Command "Compress-Archive -Path '%DoxygenDir%\*' -DestinationPath '%ZipFile%' -Force"
			echo Doxygen folder zipped to: %ZipFile%
		) else (
			echo Doxygen folder not found, skipping zip.
		)
	) else (
		echo Doxygen output folder not found at: %DoxyOutput%
	)
) else (
	echo Doxygen dont exists
)


echo Post-build steps completed successfully.
endlocal
