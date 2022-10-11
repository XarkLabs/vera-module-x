# Win32 build of Frank van den Hoef's VERA programming utility (w/o libUSB)

Converted by [Xark](https://hackaday.io/Xark) from the original.

This was created to hopefully make it easier for Windows users to program the Commander X-16 VERA module firmware (if needed).  Doing this from Windows with (hopefully) fewer driver hassles than libUSB and using common inexpensive FTDI USB UART modules.  It may not be very speedy...

# Tools Required for Building

Install [MSYS2 Platform](https://www.msys2.org/wiki/MSYS2-installation/) (I used 64-bit, but I believe either would work).

Update MSYS2 as instructed with repeated `pacman -Syuu` until everything is updated.

From within MSYS2-mingw32 prompt (see MSYS2 Start memu) install needed tools:

```sh
pacman -S tar make`
pacman -S mingw-w64-i686-toolchain
```

Then `make` in the `programer/programmer-tool-win32` directory.

You may also need to install the [FTDI D2XX drivers](https://ftdichip.com/drivers/d2xx-drivers/) and possibly copy `ftd2xx.dll` into the same folder as the executable, if it complains (until a proper installer created).




