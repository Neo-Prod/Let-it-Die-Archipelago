# V1.1 source snapshot

Native/ contains all 5 C files and 164 headers from the editable native tree, including proxy.c. APWorld/ contains the complete current Python package and its data/docs. Config/ contains the release YAML.

Build/ contains the native translation unit, explicit export list, build settings, and build.py. Use Python 3.10+ and Zig 0.15.2 on Windows. Choose an output directory outside this immutable release:

```powershell
python "Code source/Build/build.py" --output D:/LID-build --native --zig C:/tools/zig/zig.exe
```

Without --native, the helper only packages the APWorld. It never installs or launches the game. Keep the supplied validated dinput8.dll: its proxy source is included for reference but this helper rebuilds only the main mod DLL. Rebuilt PE files may differ in paths/metadata; matching release hashes is not promised.

These are integration sources and reconstructed logic, not the original LET IT DIE source code. UnrealScript exports, game executables, master databases, saves, logs, credentials, external compilers and Archipelago itself are not redistributed. TuberRooms.py and ChestRooms.py record their research provenance and limits. Work on a separate editable copy, never in Version Stable.

Build settings enable the exact R153 material/KC features and bounded diagnostics. Unused R146 pickup draft headers are retained in the complete source snapshot but PICKUP_ENABLED=0: they are not the active KC implementation.

Version1.1 enables native AP notifications and scoped character read validation, retains material/KC optimizations, and disables the temporary R16 watcher comparison profiler. Build settings are authoritative for reproducing the production variant.

Version 1.11 updates progression_reservation.h and retains the exact tested R161 diagnostic build (R16 profiler enabled). The current build_settings.json is authoritative; it supersedes the profiler-off description for the earlier V1.1 release.
