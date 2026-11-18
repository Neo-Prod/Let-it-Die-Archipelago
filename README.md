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

## Included files and requirements

Only the `Archipelago` folder, `dinput8.dll`, `let_it_die.apworld` and `LetItDie.yaml` are needed for the mod installation, in addition to LET IT DIE and Archipelago themselves. Universal Tracker is optional and installed separately.

The YAML includes all options with default values. `manifest.json` and `SHA256SUMS.txt` are optional integrity information, not game requirements. No saves, logs or personal connection details are included.

## Four goals

Choose the boss that ends your run:

- Max Sharp
- Jackson
- Crowley
- Taro Gunkanyama

Checks above your chosen goal's floor are excluded from the run.

## Customizable starting fighter

Start with a **Grade 6 fighter**, with starting stats and gains per progressive item configured in your YAML. This includes the six base stats, **Death Bag capacity**, **skill slots**, and **Rage Gauge capacity**.

## Blueprints, decals, and equipment

Equipment blueprints and premium decals are available as Archipelago items. Choose the **maximum equipment tier** and which equipment factions are included in your run:

- D.O.D. Arms
- War Ensemble
- Candle Wolf
- M.I.L.K.

Equipment is progressive: each receipt unlocks the next blueprint in its equipment family, available to purchase at **+0**. Previously unlocked equipment remains available.
Equipment catalogue tiers now range from 1 to 9. `maximum_equipment_tier` defaults to 6. 
`equipment_upgrade_steps` defaults to 0 and accepts 0 to 4 extra receipts between tier-table milestones. 
For example, 2 gives +0, +2, +4, then the next equipment rank. 
Uncapped milestones follow the tier table, including +5/+10/+15/+20 where applicable.

An optional setting groups each armor set into one progressive item family, unlocking its **head, body, and leg pieces together**. When disabled, the three pieces progress separately.

## Filler

- KC (Kill Coins) 2.000, 10.000 and 50.000.

## Checks and locations

- **Bosses:** defeat bosses and minibosses.
- **Exploration:** visit each included area for the first time.
- **Haters:** defeat the Hater in each eligible area.
- **Tubers:** configure the number of checks per eligible version for each of the six families in your YAML: **Scratch, Hovering, Bullet, Bone, Reversal, and Treasure**.
- **Screamers:** the remaining checks are distributed as evenly as possible between the included **10-floor bands**. Each eligible Screamer kill has a configurable chance to award a check for its band.

An optional **Screamer pity** setting guarantees a check after `ceil(100 / check chance)` eligible kills without a check. Receiving a check resets the counter. For example, at a **25%** chance, the fourth eligible kill is guaranteed to award a check if the first three did not. At **50%**, the guarantee also occurs on the second kill.

- **Chest Sanity** red, **silver, and gold chests, for each **10-floor bands**.
- **Marerial Sanity** copper, aluminium, iron, oil, wood, cloth and faction metals.
- **ShopSanity**

## AI Disclosure

AI was used in this project mainly for decompilation and recovering classes and native functions using Guidra and UE Explorer. 
It was also used to run tests to spot potential errors to avoid having to test each change in-game, which saved a lot of time.
