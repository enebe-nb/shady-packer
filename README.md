# Shady Packer
A specific fighting game resource manipulation tools

## Building

### Preparing the build envoirment
This tools uses [CMake](http://cmake.org) to create any necessary files.

Its possible to build it using MSVC, but the Gui tools requires a working gtkmm
dev stack that is hard to get in MSVC, so we are using Mingw build method.

First download and install MSYS from:
(https://sourceforge.net/projects/mingw/files/latest/download?source=files)

Run the newly installed `MSYS2 MinGW 32-bit` application. Execute the
following command: `pacman -S git base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-gtkmm3`

### Downloading and building the tools
Run the `MSYS2 MinGW 32-bit` application. Execute the following commands:

```
git clone git://github.com/enebe-nb/shady-packer.git
mkdir shady-packer/build
cd shady-packer/build
cmake .. -G "MSYS Makefiles" -DDISABLE_SYMLINK=TRUE -DCMAKE_INSTALL_PREFIX=../deploy
make install
```

After that you will have a working copy of shady packer tools in `shady-packer/deploy`.

## Using

### Shady Viewer

This is a Gui tool allowing to open the game package files and view its resources.
You can click in `File > Append Package` and select your `th123a.dat` and double click
in any resource files loaded by the application.

[Soon refer to wiki](https://github.com/enebe-nb/shady-packer/wiki)

### Shady Cli

This is a command-line tool to manipulate the game resources. You can use `shady-cli help`
to get a list of commands or `shady-cli help <command>` to get a detailed help.

[Soon refer to wiki](https://github.com/enebe-nb/shady-packer/wiki)
