# Shady Packer
A specific fighting game resource manipulation tools

## Downloading ready-to-use version
Go to (https://github.com/enebe-nb/shady-packer/releases) and download your preferred version.

Modules are on `shady-loader` folder and executables on `bin` folder.

## Using

### Shady Viewer
This is a Gui tool allowing to open the game package files and view its resources.
You can click in `File > Append Package` and select your `th123a.dat` and double click
in any resource files loaded by the application.
[More info on wiki](https://github.com/enebe-nb/shady-packer/wiki/tools-viewer)

### Shady Cli
This is a monolithic(don't require extra dlls) command-line tool to manipulate the
game resources. You can use `shady-cli help` to get a list of commands or
`shady-cli help <command>` to get a detailed help.
[More info on wiki](https://github.com/enebe-nb/shady-packer/wiki/tools-cli)

### Shady Loader
This is a module to load multiple custom packages into the game.
[More info on wiki](https://github.com/enebe-nb/shady-packer/wiki/tools-loader)

### Shady Lua
This is a module to run lua scripts in the game.
[More info on wiki](https://github.com/enebe-nb/shady-packer/wiki/tools-lua)

## Building
This tools uses [CMake](http://cmake.org) to create any necessary files.

Its possible to build it using MSVC, but the Gui tools requires a working gtkmm
dev stack that is hard to get in MSVC, so we are using Mingw build method.

Its possible to custom the build with the following cmake options:
`SHADY_ENABLE_CLI`; `SHADY_ENABLE_GUI`; `SHADY_ENABLE_TEST`; `SHADY_ENABLE_MODULE`.

Modules only builds on 32-bit Windows targets.

### On MSYS
First download and install MSYS from: (https://msys2.org).
Its recommended to use `MSYS2 MinGW 32-bit` application.

Use the `pacman` command to setup the environment.
The basic libs are `git`, `base-devel`, `toolchain` and `cmake`.
For shady-viewer, the `gtkmm3` also is required.

Command Example:
`pacman -S git base-devel mingw-w64-i686-toolchain mingw-w64-i686-cmake mingw-w64-i686-gtkmm3`

To build the tools, clone the repository and use cmake to generate the make files.
Example:
```sh
git clone git://github.com/enebe-nb/shady-packer.git
mkdir shady-packer/build
cd shady-packer/build
cmake .. -G "MSYS Makefiles" -DCMAKE_INSTALL_PREFIX=../deploy
make install
```
After that you will have a working copy of shady packer tools in `shady-packer/deploy`.

### On Visual Studio
Remember to use the `Visual Studio Installer` to install the `C++ CMake tools for Windows`
in `C++ build tools` section.
The gtkmm doesn't have good support on VS, so the shady-viewer build is disabled by default.

To build the tools, clone the repository and use cmake to generate the project files.
Example:
```sh
git clone git://github.com/enebe-nb/shady-packer.git
mkdir shady-packer/build
cd shady-packer/build
cmake .. -G 'Visual Studio 16 2019' -A Win32
```
After that the project files can be found in `shady-packer/build`.

### On Linux
Similar to MSYS method, but modules won't build.
It will use pkgconfig to find gtkmm, so setup it using your distro.
`cmake .. -DCMAKE_INSTALL_PREFIX=../deploy` can be used as cmake command.