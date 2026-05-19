# Aerosynth-GTAV

Synthetic data pipeline source for GTA V

## Dependencies

### Build

* CMake (developed on 4.3.2)
* MVSC (developed on MSVC 19.51.36243.0)
* Ninja (optional - developed on 1.13.2)

### Compile-time

* ScriptHookV SDK 1.0.617.1a (vendored)
* LodePNG (vendored)

## Building

The project assumes you're building on Windows with an MVSC toolchain.  Initialize submodules before building (`git submodule update --init`).

### Using MsBuild:

```powershell
cmake -B build -DCMAKE_INSTALL_PREFIX="D:\SteamLibrary\steamapps\common\Grand Theft Auto V"
cmake --build build --config Release
```

### Using Ninja:

Ninja respects `CMAKE_EXPORT_COMPILE_COMMANDS`, which can be useful for getting `clangd` intellisense to work.

```powershell
# Launch the Visual Studio dev shell (adjust for your toolchain)
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64

cmake -B build -G "Ninja" -DCMAKE_INSTALL_PREFIX="D:\SteamLibrary\steamapps\common\Grand Theft Auto V"
cmake --build build --config Release
```

## Installing

[ScriptHookV](https://www.dev-c.com/gtav/scripthookv/) must be installed in your game directory.

The `CMakeLists.txt` includes an install target which will install the mod to your game directory given you provided it at configure time with `-DCMAKE_INSTALL_PREFIX`.

```powershell
cmake --install build
```

Alternatively, manually copy the `aerosynth_gtav.asi` file to your game directory.

## Miscellaneous

Killing the game (useful when developing remotely over SSH):
```
taskkill /IM gta5.exe /F  
```