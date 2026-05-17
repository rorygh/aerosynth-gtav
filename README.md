# Aerosynth-GTAV

Synthetic data pipeline source for GTA V

## Dependencies

* CMake (developed on 4.3.2)
* MVSC (developed on MSVC 19.51.36243.0)

The project includes a vendored copy of ScriptHookV SDK 1.0.617.1a.

## Building

```
cmake -B build -DCMAKE_INSTALL_PREFIX="D:\SteamLibrary\steamapps\common\Grand Theft Auto V"
cmake --build build --config Release
```

## Installing

(ScriptHookV)[https://www.dev-c.com/gtav/scripthookv/] must be installed in your game directory.

The `CMakeLists.txt` includes an install target which will install the mod to your game directory given you provided it at configure time with `-DCMAKE_INSTALL_PREFIX`.  Alternatively, manually copy the `aerosynth_gtav.asi` file to your game directory.