:start

xcopy /d/e/c/i/h/r/Y "C:\Users\guozs\Documents\WeChat Files\gzshun\Video\*.mp4" "E:\small_video\%date:~0,4%%date:~5,2%%date:~8,2%"
choice /t 30 /d y /n >nul
goto start