@echo off

rem --------------------------------------------------------------------------
rem                        COMPILATION
rem --------------------------------------------------------------------------

set SDLPath=C:\SDL2-2.0.7\
set SDLBinPath=%SDLPath%\lib\x64\
rem set SDLNetPath=C:\SDL2_net-2.0.1\
rem set SDLNetBinPath=%SDLNetPath%\lib\x64\

set UntreatedWarnings=/wd4100 /wd4244 /wd4201 /wd4127 /wd4505 /wd4456 /wd4996 /wd4003 /wd4706
set CommonCompilerDebugFlags=/MT /O2 /Oi /fp:fast /fp:except- /Zo /Gm- /GR- /EHa /WX /W4 %UntreatedWarnings% /Z7 /nologo /I %SDLPath%\include\ /I %STBPath% /I %RivtenPath%
set CommonLinkerDebugFlags=/incremental:no /opt:ref /subsystem:console %SDLBinPath%\SDL2.lib %SDLBinPath%\SDL2main.lib /ignore:4099

pushd ..\build\
cl %CommonCompilerDebugFlags% ..\code\ray.cpp /link %CommonLinkerDebugFlags%
popd

rem --------------------------------------------------------------------------
echo Compilation completed...
