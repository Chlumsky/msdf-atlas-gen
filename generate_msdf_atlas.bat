@echo off
setlocal enabledelayedexpansion

rem Paths to msdf-atlas-gen executable and font file
set "MSDF_EXE=C:\Users\DJBen\Downloads\msdf-atlas-gen\msdf-atlas-gen.exe"
set "FONT=C:\Users\DJBen\Downloads\SF-Pro-Display-Regular.otf"
set "ARIAL_FONT=C:\Windows\Fonts\arial.ttf"

rem Output folder next to this script
set "OUTDIR=%~dp0output"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set "BASENAME=SF-Pro-Display_msdf"
set "BASENAME2=Arial_msdf"

echo Generating MSDF atlas and JSON...
"%MSDF_EXE%" ^
  -font "%FONT%" ^
  -glyphset "%~dp0scripts\glyph_ranges.txt" ^
  -type msdf ^
  -size 64 ^
  -pxrange 4 ^
  -imageout "%OUTDIR%\%BASENAME%.png" ^
  -json "%OUTDIR%\%BASENAME%.json"

if errorlevel 1 (
  echo msdf-atlas-gen failed.
  exit /b 1
) else (
  echo Done.
  echo Atlas: "%OUTDIR%\%BASENAME%.png"
  echo JSON : "%OUTDIR%\%BASENAME%.json"
)

echo.
if exist "%ARIAL_FONT%" (
  echo Generating MSDF atlas and JSON for Arial...
  "%MSDF_EXE%" ^
    -font "%ARIAL_FONT%" ^
    -glyphset "%~dp0glyphsets\Arial.glyphset.txt" ^
    -type msdf ^
    -size 48 ^
    -pxrange 4 ^
    -imageout "%OUTDIR%\%BASENAME2%.png" ^
    -json "%OUTDIR%\%BASENAME2%.json"

  if errorlevel 1 (
    echo msdf-atlas-gen failed for Arial.
    exit /b 1
  ) else (
    echo Done Arial.
    echo Atlas: "%OUTDIR%\%BASENAME2%.png"
    echo JSON : "%OUTDIR%\%BASENAME2%.json"
  )
) else (
  echo WARNING: Arial font not found at "%ARIAL_FONT%". Skipping Arial.
)

endlocal
