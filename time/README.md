View UNIX-time as a date-time string.<br>

### How to build by mingw64
```
set PATH=c:\mingw64\mingw64\bin\;%PATH%
gcc -Wl,--kill-at -shared -static ./src/main.c -o time.vvp -m64 -s -Os -lgdi32 -lcomctl32 -luxtheme
```