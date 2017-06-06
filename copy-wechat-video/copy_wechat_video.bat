:start

xcopy /d/e/c/i/h/r "C:\Users\guozs\Documents\WeChat Files\gzshun\Video" "E:\small_video\%date:~0,4%%date:~5,2%%date:~8,2%"
choice /t 30 /d y /n >nul
goto start