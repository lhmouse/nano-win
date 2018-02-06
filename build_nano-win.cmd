@echo off

call mingw 32
bash build_nano-win.sh i686

call mingw 64
bash build_nano-win.sh x86_64

bash package_nano-win.sh
