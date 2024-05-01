@echo off

call mingw ucrt64
bash build_nano-win.sh x86_64

call mingw 32
bash build_nano-win.sh i686

bash package_nano-win.sh
