# LET IT DIE Archipelago

## 1. Install the mod

Close LET IT DIE. Copy **`dinput8.dll`** and the supplied **`Archipelago`** folder into the game's `Binaries/Win64/` folder:

```text
LET IT DIE/
  Binaries/
    Win64/
      dinput8.dll
      Archipelago/
        LetItDieArchipelago.dll
```

Keep the name `dinput8.dll`. Do not rename it to `dinput.dll`.

Copy **`let_it_die.apworld`** into Archipelago's **`custom_worlds`** folder, then restart Archipelago Launcher. Copy **`LetItDie.yaml`** into the **`Players`** folder used for generation. Set your slot name and options, then generate and host your Archipelago game.

Archipelago must already be installed. The in-game button looks for its launcher in `C:\Archipelago`, `D:\Archipelago`, another drive-root `Archipelago` folder, or `C:\ProgramData\Archipelago`.

## 2. Back up your existing save

With the game closed, open the `Savedata` folder inside the LET IT DIE installation folder. Your save is inside a profile subfolder:

```text
LET IT DIE/Savedata/<profile>/brggame.sav
```

**If a save already exists, copy the entire `Savedata` folder somewhere safe outside the game folder first.** This also preserves any backup files. Check that your copy contains `brggame.sav` before proceeding.

Once the backup is confirmed, move the existing profile folder out of `Savedata` so the game can create a fresh save. Keep your backup: do not delete it or overwrite it with the new game. Do not leave extra profile copies inside `Savedata`; the AP client expects one active native save profile.

## 3. Prepare the starting save

1. Launch LET IT DIE and start a **new normal game without connecting to Archipelago yet**.
2. Play the tutorial until the **mandatory death of the first fighter**.
3. Continue until you reach the **Waiting Room with the second fighter**.
4. **Quit the game at this point**, allowing it to save normally.

This is the starting save needed by the Archipelago client. You do not need someone else's save.

## 4. Connect and play

Launch LET IT DIE again. From the **title menu**, use the **Archipelago connection option** and enter the connection details for your generated game.

Wait for the connection confirmation, then start the game normally. You can now play your Archipelago run.

## Updating an existing AP run

Merge the new `Archipelago` folder with your installation. **Do not delete `Archipelago/slots` or repeat the fresh-save procedure when continuing an existing AP run.**

This release fixes reconnecting to an existing AP save. Removing the 12 Hater checks from Forcemen rooms requires a new generation; their room visit checks remain available.

## Included files and requirements

Only the `Archipelago` folder, `dinput8.dll`, `let_it_die.apworld` and `LetItDie.yaml` are needed for the mod installation, in addition to LET IT DIE and Archipelago themselves. Universal Tracker is optional and installed separately.

The YAML includes all options with default values. `manifest.json` and `SHA256SUMS.txt` are optional integrity information, not game requirements. No saves, logs or personal connection details are included.

## AI Disclosure

AI was used in this project mainly for decompilation and recovering classes and native functions using Guidra and UE Explorer. 
It was also used to run tests to spot potential errors to avoid having to test each change in-game, which saved a lot of time.
