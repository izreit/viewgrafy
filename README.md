Viewgrafy
=======

`viewgrafy` shows an image file as a viewgraph.

Choose the image you prefer to overlay it on the display of your PC!
It will be a feast for the eyes any day now while wallpapers get covered up by windows most of the time.

# Bulid

1. Install Cygwin and mingw-gcc-g++ on it.
2. Just `make` at the top of the repository.
3. Now you have the binary, viewgrafy/viewgrafy.exe.

Currently the only compiler we tested is i686-pc-mingw32-g++ 4.7.3 on Windows 7.
When you use other one, try `CROSS_PREFIX` option (e.g. `make CROSS_PREFIX=i686-w64-mingw-`)

# Usage

Just run viewgrafy/viewgrafy.exe.

No installation required. No registry touched.
When it is the first run you will be asked to choose an image file.
Once executed, it stays in the task tray and you can change the image file and its opacity.

Just fun!

# License

MIT. See LICENSE.

