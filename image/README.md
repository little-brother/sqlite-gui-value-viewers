Basic image viewer<br>

### How to build by mingw64
```
set PATH=c:\mingw64\mingw64\bin\;%PATH%
g++ -Wl,--kill-at -shared -static ./src/main.cpp -o image.vvp -m64 -s -Os -lgdi32 -lcomctl32 -luxtheme -lgdiplus -lole32
```