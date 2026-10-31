# Easyflash3 toolset for macOS

This is the complete toolset for macOS and worfklow guide (macOS host oriented) to manufacture EasyFlash3 cartridges.

If this doesn't work for you, please bug **me** and not Skoe since he wasn't involved in this work at all.

## Dependencies

### Xcode command line tools

```
xcode-select --install
```

### Homebrew

Follow the instructions at [brew.sh](https://brew.sh):

```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### Cross dev tools and ftdi libs:

```
brew install acme cc65 libftdi libusb cmake confuse imagemagick python
```

(`confuse` and `cmake` are required for the FT245 tools, see below)

### Static wxWidgets

The host tools link Homebrew's `libftdi` and `libusb` static archives. wxWidgets
must also be built statically: Homebrew's `wxwidgets` formula currently ships
shared libraries only. Build the wxWidgets archives from source once:

```
WX_VERSION=3.3.3
WX_WORK="$HOME/.cache/easyflash/wx-$WX_VERSION"
WX_PREFIX="$HOME/.local/easyflash/wx-static"
mkdir -p "$WX_WORK"
curl -L "https://github.com/wxWidgets/wxWidgets/releases/download/v$WX_VERSION/wxWidgets-$WX_VERSION.tar.bz2" -o "$WX_WORK/wxWidgets-$WX_VERSION.tar.bz2"
tar -xjf "$WX_WORK/wxWidgets-$WX_VERSION.tar.bz2" -C "$WX_WORK"
(
cd "$WX_WORK/wxWidgets-$WX_VERSION"
./configure --prefix="$WX_PREFIX" --with-osx_cocoa \
    --disable-shared --disable-monolithic --enable-xrc --enable-html \
    --with-libpng=builtin --with-libjpeg=builtin --with-libtiff=builtin \
    --with-libwebp=builtin --with-regex=builtin --with-zlib=builtin \
    --with-expat=builtin --without-libcurl --with-macosx-version-min=10.10 \
    --disable-tests --disable-precomp-headers
make -j"$(sysctl -n hw.logicalcpu)"
make install
)
```

`make` in EasyTransfer and EasySplit uses this prefix by default. To use a
different static wxWidgets build, pass `WX_CONFIG=/path/to/wx-config`.
Static linking removes Homebrew library dependencies at runtime, but it does
not make a Tahoe arm64 build portable to older macOS releases or Intel Macs.
Static archives carry their own minimum-OS metadata; support for an older
system requires building every dependency with a matching SDK and architecture.

### exomizer

The Homebrew recipe for `exomizer` is broken (segfaults), so get the exomizer binary from here: [www.popelganda.de/relaunch64.html](http://www.popelganda.de/relaunch64.html).

Put it somewhere in the `$PATH`.

### cartconv

This is a crt format conversion tool that comes with Vice.

Not strictly needed in the manufacturing process but it needs to be in the `$PATH` if you want to build your own menu.crt image.

## easp

`easp` is the tool required to program the CPLD, this is the very first step to do on an new EF3 before flashing the menu. 

On macOS, `easp` links Homebrew's `libftdi` and `libusb` static archives, so it
does not need those Homebrew libraries at runtime.

To compile:

```
cd ManufacturerTools/easp-2014-01-09
make
```

To program the CPLD **put the EF3 jumpers in the PROG position**, plug the EF3 in the C64, plug in the USB cable, power the C64 on, then:

```
easp -p 0x6001 -v <svf_file> 
```

If this is a cartridge where the PID has already been changed (explained below):

```
easp -p 0x8738 -v <svf_file> 
```

For the original `easp` distribtuion including win32 and linux binaries go to:

[https://bitbucket.org/skoe/easp/downloads/](https://bitbucket.org/skoe/easp/downloads/)


## FT245 tools

In order for macOS (and Windows/Linux) to recognize an EasyFlash 3 as such, the product identifier (PID) of the FT245 must be changed from the default (0x6001) to the one expected by the EF3 driver and tools (0x8738).

The official instructions by Skoe explain how to do it with the official, Windows-only, FTDI tools

However, this can be done without leaving macOS with two example tools that come with the libftdi library:

 * `build/examples/eeprom`: dumps eeprom's contents
 * `build/ftdi_eeprom/ftdi_eeprom`: to customize the PID, S/N, etc.

To compile:

```
cd ManufacturerTools/FT245/libftdi1-1.4
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX="$HOME/.local/easyflash/ft245" \
    -DEASYFLASH_STATIC_TOOLS=ON -DFTDIPP=OFF -DBUILD_TESTS=OFF \
    -DDOCUMENTATION=OFF ../
make
```

The command-line tools link against static `libftdi`, `libusb`, and `libconfuse`
archives. Apple system frameworks remain runtime dependencies.

**NOTE: the EF3 need not be plugged into the C64 for neither tool to work.**

### eeprom

To dump raw eeprom contents (for backup purposes, or to test communication with the FT245 chip):

```
eeprom -v 0x0403 -p 0x6001 >dump.eeprom
```

or

```
eeprom -v 0x0403 -p 0x8738 >dump.eeprom
```

... depending on the actual PID of the FT245.


### ftdi_eeprom

To read the EEPROM and parse the FT245 config (this can be done without plugging the EF3 in the C64):

```
ftdi_eeprom --device i:0x0403:0x6001 --read-eeprom ef3.conf
```

To reprogram the PID, serial number and creator string into the FT245 EEPROM:

First edit the provided `ManufactureTools/FT245/ef3.conf` and replace the creator string and serial number with your own, then:

```
./ftdi_eeprom --flash-eeprom ef3.conf
```

## easytransfer and ef3xfer tools

`easytransfer` is a GUI tool to upload CRT files to an EF3, run PRGs and format/dump d64 images to a real floppy connected to the C64 via USB.

`ef3xfer` is its CLI counterpart.

These tools will only work if the FT245 PID has been changed to 0x8738 as explained above.

To compile:

```
cd EasyTransfer
make
```

The results are `easytransfer` (GUI tool) and `ef3xfer` (CLI tool) in `EasyTransfer/out/easytransfer/`.

Run the tools with the EF3 plugged in the C64, the C64 on, the USB cable plugged in and the **EF3 jumpers in the DATA position**.

Examples for `ef3xfer`:

```
ef3xfer -c cart.crt  # write cart.crt to cart
ef3xfer -x prog.prg  # upload and run prog.prg
```

## Other tools

### EasyProg

Not strictly needed, but you might want to customize and/or recompile the C64-side flashing tool (EasyProg):

```
cd EasyProg
make
```

The result is `easyprog.prg`

### EasySplit

```
cd EasySplit
make
```

The result is `EasySplit/out/easysplit/easysplit`.

### Menu image (menu-init.crt)

First:

```
cd EasySDK/tools
make
```

The result is `bin2efcrt`, required by the next step:

```
cd EF3BootImage
make
```

The results are `ef3-init.crt` and `ef3-menu.crt` in `EF3BootImage/`.

Use the former to completely overwrite the menu and slot data. Use the latter to just update the menu while keeping data (kernals, slot names and such).

As an example, the menu image in this repo comes modified with a light green background.

## Menu background image

EF3's menu background image is in `EF3BootImage/efmenu/src/background.iph` in [Interpaint](https://csdb.dk/release/?id=13467) format.

For Interpaint to read the file:

 1. It must end with a right-padded ".HRES" suffix
 2. Must be of type PRG (not SEQ)
 3. Uncheck the PROJECT -> MULTICOL option in Interpaint.

The other IPH files in that directory seem to be older versions of the background.

A ready-made D64 with Interpaint and all current backgrounds is in `ManufacturerTools/Interpaint`.

# Thanks and related info

 * [Skoe](http://skoe.de/easyflash/doku.php)
 * [Linux build directions at c64-wiki.de](https://www.c64-wiki.de/wiki/EasyFlash³/Sourcecode_kompilieren)
 * [This fantastic mod to add support for the Final Cartridge 3 by Kim Jorgensen](http://github.com/KimJorgensen/easyflash) 
