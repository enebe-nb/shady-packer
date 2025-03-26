# Shady Packer
A specific fighting game resource manipulation tools

## Downloading ready-to-use version
Go to (https://github.com/enebe-nb/shady-packer/releases) and download your preferred version.

Modules are on `shady-loader` folder and executables on `bin` folder.

## Using

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

Its possible to custom the build with the following cmake options:
`SHADY_ENABLE_EXECUTABLE`; `SHADY_ENABLE_TEST`; `SHADY_ENABLE_MODULE`.

Modules only builds on 32-bit Windows targets.

### On Visual Studio
Remember to use the `Visual Studio Installer` to install the `C++ CMake tools for Windows`
in `C++ build tools` section.

To build the tools, clone the repository and use cmake to generate the project files.
Example:
```sh
git clone git://github.com/enebe-nb/shady-packer.git
cd shady-packer
git submodule update --init --recursive
mkdir build
cd build
cmake .. -G 'Visual Studio 16 2019' -A Win32
cmake --build . --config Release
```
After that the project files can be found in `shady-packer/build`.
