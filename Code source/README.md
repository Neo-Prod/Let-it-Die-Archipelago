# Version 1.2 integration source snapshot

Native/ contains all 5 C files and 260 headers from Working_Copy, including proxy.c. The supplied binary is the exact user-tested R326 DLL; the APWorld includes R327 default options. APWorld/ contains the complete tested Python package and its catalogue data. Config/ contains the release YAML with all 67 options.

Build/ contains the translation unit, export list, explicit feature/SEH settings and portable build.py. Use Python 3.10+ and Zig 0.15.2 on Windows. Copy the source to an editable workspace and put build outputs outside the release directory:

```powershell
python "Code source/Build/build.py" --output D:/LID-build --native --zig C:/tools/zig/zig.exe
```

Without --native, the helper only packages the APWorld. It does not install or launch the game. Keep the supplied validated dinput8.dll; proxy.c is included for reference and the helper rebuilds the main mod DLL only. PE metadata/paths can differ, so matching release file hashes is not promised. The explicit build_settings.json is authoritative and retains the exact tested diagnostic features.

These are integration sources and reconstructed logic, not the original LET IT DIE source code. Game executables/assets, master databases, saves, private research, logs, credentials, compilers and Archipelago itself are excluded. Version Stable is append-only: never build into or modify an archived release.
