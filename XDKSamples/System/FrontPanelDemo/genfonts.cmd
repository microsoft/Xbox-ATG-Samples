
set RASTERFONTGEN=..\..\Tools\RasterFontGen\x64\Debug\RasterFontGen.exe

set ANSI_RANGES=-cr:0x0020-0x007E -cr:0x00A0-0x00FF

REM Lucida Console
%RASTERFONTGEN% -tf "Lucida Console" -h 12 -of "assets\LucidaConsole12.rasterfont" -ow
%RASTERFONTGEN% -tf "Lucida Console" -h 16 -of "assets\LucidaConsole16.rasterfont" -ow
%RASTERFONTGEN% -tf "Lucida Console" -h 24 -of "assets\LucidaConsole24.rasterfont" -ow
%RASTERFONTGEN% -tf "Lucida Console" -h 32 -of "assets\LucidaConsole32.rasterfont" -ow
%RASTERFONTGEN% -tf "Lucida Console" -h 64 -of "assets\LucidaConsole64.rasterfont" -ow

REM Segoe UI
%RASTERFONTGEN% -tf "Segoe UI" -h 12 -of "assets\Segoe_UI12.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Segoe UI" -h 16 -of "assets\Segoe_UI16.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Segoe UI" -h 24 -of "assets\Segoe_UI24.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Segoe UI" -h 32 -of "assets\Segoe_UI32.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Segoe UI" -h 64 -of "assets\Segoe_UI64.rasterfont" -ow %ANSI_RANGES%

REM Segoe UI Bold
%RASTERFONTGEN% -tf "Segoe UI" -w BOLD -h 12 -of "assets\Segoe_UI_bold12.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Segoe UI" -w BOLD -h 16 -of "assets\Segoe_UI_bold16.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Segoe UI" -w BOLD -h 24 -of "assets\Segoe_UI_bold24.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Segoe UI" -w BOLD -h 32 -of "assets\Segoe_UI_bold32.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Segoe UI" -w BOLD -h 64 -of "assets\Segoe_UI_bold64.rasterfont" -ow %ANSI_RANGES%

REM Arial Narrow
%RASTERFONTGEN% -tf "Arial Narrow" -h 12 -of "assets\ArialNarrow12.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Arial Narrow" -h 16 -of "assets\ArialNarrow16.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Arial Narrow" -h 24 -of "assets\ArialNarrow24.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Arial Narrow" -h 32 -of "assets\ArialNarrow32.rasterfont" -ow %ANSI_RANGES%
%RASTERFONTGEN% -tf "Arial Narrow" -h 64 -of "assets\ArialNarrow64.rasterfont" -ow %ANSI_RANGES%


REM Symbols

set SYMBOL_RANGES=-cr:0xE3AF-0xE3B2 -cr:0xE48B-0xE48C

%RASTERFONTGEN% -tf "Segoe Xbox MDL2 Assets" -h 16 -of "assets\Symbols16.rasterfont" -ow %SYMBOL_RANGES%
%RASTERFONTGEN% -tf "Segoe Xbox MDL2 Assets" -h 32 -of "assets\Symbols32.rasterfont" -ow %SYMBOL_RANGES%


