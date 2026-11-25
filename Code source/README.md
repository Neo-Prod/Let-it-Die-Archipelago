# Version 1.3 integration source snapshot

Native/ contains all 5 C files and 280 headers from Working_Copy, including proxy.c. APWorld/ contains the complete package (84 Python modules and 10 JSON catalogues plus its documentation). Config/ contains the release YAML with all73 options.

Build/ contains the translation unit, export list, explicit feature/SEH settings and portable build.py. Use Python3.10+ and Zig0.15.2 on Windows. Copy the source into an editable workspace; keep outputs outside the archived release:

```powershell
python "Code source/Build/build.py" --output D:/LID-build --native --zig C:/tools/zig/zig.exe
```

Without --native, the helper only packages the APWorld. It does not install or launch the game. Keep the supplied validated dinput8.dll; proxy.c is included for reference and this helper rebuilds the main mod DLL only. File hashes may differ with build paths/PE metadata; compiled-code/source parity is checked for this snapshot. build_settings.json contains all enabled features, including Floor and Mastery.

These are integration sources and reconstructed logic, not the original LET IT DIE source code. Executables/assets, master databases, saves, private research/logs/credentials, compilers and Archipelago itself are excluded. Version Stable is append-only: never build into or modify an archived release.
