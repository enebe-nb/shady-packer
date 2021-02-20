# Shady Packer
A specific fighting game resource manipulation tools

## Downloading ready-to-use version
Go to (https://github.com/enebe-nb/shady-packer/releases) and download your preferred version.

Modules are on `shady-loader` folder and executables on `bin` folder.
- shady-viewer: GUI tool for manipulating data files.
- shady-cli: monolithic(don't require extra dlls) console tool like the packer.
- shady-loader: Module for automatic loading of custom files in the game.
- shady-lua: Module for lua script execution inside the game.

## Building

### Preparing the build envoirment
This tools uses [CMake](http://cmake.org) to create any necessary files.

Its possible to build it using MSVC, but the Gui tools requires a working gtkmm
dev stack that is hard to get in MSVC, so we are using Mingw build method.

First download and install MSYS from: (https://msys2.org)

Run the newly installed `MSYS2 MinGW 32-bit` application. Execute the
following command: `pacman -S git base-devel mingw-w64-i686-toolchain mingw-w64-i686-cmake mingw-w64-i686-gtkmm3`

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

[More info on wiki](https://github.com/enebe-nb/shady-packer/wiki/tools-viewer)

### Shady Cli

This is a command-line tool to manipulate the game resources. You can use `shady-cli help`
to get a list of commands or `shady-cli help <command>` to get a detailed help.

[More info on wiki](https://github.com/enebe-nb/shady-packer/wiki/tools-cli)

### Shady Loader

This is a helper tool to load multiple custom packages into the game.

[More info on wiki](https://github.com/enebe-nb/shady-packer/wiki/tools-loader)
