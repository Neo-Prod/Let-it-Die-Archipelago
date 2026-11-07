"""Build the LET IT DIE Archipelago starter save from the user's post-tutorial save.

The reference save already represents the exact desired Waiting Room state.
For that reason this module deliberately does not synthesize tutorial flags or
replace the freezer. It preserves the shell, converts the selected fighter
(state == "USE"), and removes inherited research only for a new AP slot.
"""

from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .SaveFile import (
    LetItDieSaveError,
    get_bodyuser_records,
    get_fighters,
    get_main_uid,
    read_brg_save,
    write_brg_save,
)


@dataclass(frozen=True)
class StarterSaveSettings:
    """Starting values mirrored from Archipelago slot_data."""

    fighter_type: str = "BAL"
    fighter_grade: int = 6

    hp: int = 200
    stm: int = 200
    strength: int = 10
    dex: int = 10
    vit: int = 10
    luck: int = 10

    skill_slots: int = 0
    death_bag: int = 1
    rage_gauge: int = 0


# Returns the fighter selected by LET IT DIE in the template save.
def _get_selected_fighter(root: dict[str, Any]) -> dict[str, Any]:
    selected = [fighter for fighter in get_fighters(root) if fighter.get("state") == "USE"]

    if len(selected) != 1:
        raise LetItDieSaveError(
            "Expected exactly one selected fighter (state='USE') in the post-tutorial template; "
            f"found {len(selected)}."
        )

    return selected[0]


# Returns the bodyuser record belonging to the selected fighter.
def _get_selected_bodyuser(root: dict[str, Any], cid: str) -> dict[str, Any]:
    matches = [row for row in get_bodyuser_records(root) if str(row.get("cid") or "") == cid]

    if len(matches) != 1:
        raise LetItDieSaveError(
            "Expected exactly one bodyuser record for the selected fighter; "
            f"found {len(matches)} for cid={cid!r}."
        )

    return matches[0]


# Converts only the already-selected fighter to the AP Grade 6 level-1 fighter.
def _configure_selected_fighter(
    root: dict[str, Any],
    settings: StarterSaveSettings,
) -> str:
    fighter = _get_selected_fighter(root)
    cid = str(fighter.get("cid") or "")

    if not cid:
        raise LetItDieSaveError("Selected fighter has no cid.")

    bodyuser = _get_selected_bodyuser(root, cid)

    # Keep identity / appearance / name / slot / state exactly as the template.
    fighter["type"] = str(settings.fighter_type)
    fighter["grade"] = int(settings.fighter_grade)
    fighter["limit_break"] = 0
    fighter["hp"] = int(settings.hp)

    # A fresh AP fighter must not inherit run experience or currencies.
    fighter["money"] = 0
    fighter["spirit"] = 0
    fighter["bloodnium"] = 0
    fighter["total_exp"] = 0
    fighter["rest_exp"] = 0
    fighter["gain_exp"] = 0
    fighter["start_exp"] = 0
    fighter["sklgauge"] = 0
    fighter["escdie"] = 0
    fighter["pause"] = ""
    fighter["abid"] = ""

    # LET IT DIE stores the visible level and the six stat progression indices
    # in bodyuser. Index 1 resolves through the AP masters.db to the configured
    # starting values (HP/STM/STR/DEX/VIT/LUK).
    bodyuser["lvl"] = 1
    bodyuser["hp"] = 1
    bodyuser["str"] = 1
    bodyuser["dex"] = 1
    bodyuser["vit"] = 1
    bodyuser["stm"] = 1
    bodyuser["luk"] = 1
    bodyuser["skill"] = 0
    bodyuser["bag"] = 0
    bodyuser["rage"] = 0

    for key in (
        "hp_bonus",
        "str_bonus",
        "dex_bonus",
        "vit_bonus",
        "stm_bonus",
        "luk_bonus",
    ):
        bodyuser[key] = 0

    if "die" in bodyuser:
        bodyuser["die"] = 0

    return cid


# Removes equipment/decal ownership from the selected fighter and makes the
# serialized Death Bag contain exactly the YAML-requested number of slots.
# LET IT DIE derives the inventory's real slot array from this list; merely
# clearing the template's 20 vanilla entries leaves a 20-slot bag in game.
def _clear_selected_fighter_loadout(
    root: dict[str, Any],
    cid: str,
    settings: StarterSaveSettings,
) -> None:
    uid = get_main_uid(root)
    soul = root.get("soul") or {}

    deathbag_by_uid = (soul.get("deathbag") or {}).get(uid)
    if not isinstance(deathbag_by_uid, dict):
        raise LetItDieSaveError("Starter save has no Death Bag table for the active user.")

    slots = deathbag_by_uid.get(cid)
    if not isinstance(slots, list):
        raise LetItDieSaveError("Starter save has no Death Bag slot list for the selected fighter.")

    capacity = int(settings.death_bag)
    if capacity < 1:
        raise LetItDieSaveError(f"starting_death_bag must be at least 1; got {capacity}.")

    # Rebuild the list instead of retaining the template's vanilla capacity.
    deathbag_by_uid[cid] = [
        {
            "uid": int(uid),
            "cid": cid,
            "slot": slot_index,
            "type": -1,
            "eid": "",
            "site": "",
            "arm_slot": -1,
        }
        for slot_index in range(capacity)
    ]

    skl = soul.get("skl") or {}
    eqskl = skl.get("eqskl")
    if isinstance(eqskl, dict):
        # Current saves key this table by uid rather than cid. Preserve the
        # table shape but clear equipped decals for the active account.
        if uid in eqskl:
            eqskl[uid] = {} if isinstance(eqskl[uid], dict) else []


# Builds the AP starter state while preserving the exact post-tutorial shell.
def build_archipelago_start(
    template_root: dict[str, Any],
    settings: StarterSaveSettings = StarterSaveSettings(),
) -> dict[str, Any]:
    root = deepcopy(template_root)

    # The supplied reference is expected to be a genuine post-tutorial save.
    # Fail closed instead of inventing progression flags when that invariant is
    # not true.
    gameflg = root.get("gameflg") or {}
    clear_flags = gameflg.get("cl") or []
    clear_map = {
        str(row.get("var")): int(row.get("value") or 0)
        for row in clear_flags
        if isinstance(row, dict) and row.get("var")
    }

    if clear_map.get("KGF_MET_TUTORIAL_CLEAR") != 1:
        raise LetItDieSaveError(
            "Template is not the validated post-tutorial save: KGF_MET_TUTORIAL_CLEAR != 1."
        )

    fighters_before = get_fighters(root)
    if len(fighters_before) != 2:
        raise LetItDieSaveError(
            "Validated post-tutorial template must contain exactly two fighters; "
            f"found {len(fighters_before)}."
        )

    first_cid = str(fighters_before[0].get("cid") or "")
    first_snapshot = deepcopy(fighters_before[0])

    selected_cid = _configure_selected_fighter(root, settings)
    _clear_selected_fighter_loadout(root, selected_cid, settings)

    # This constructor is called only for a new AP slot. Do not migrate or
    # erase research on an existing slot when reconnecting.
    soul = root.get("soul")
    if not isinstance(soul, dict):
        raise LetItDieSaveError("Starter soul has an unexpected format.")
    research = soul.get("partresearch")
    if research is None:
        soul["partresearch"] = {"user": []}
    elif not isinstance(research, dict) or not isinstance(research.get("user", []), list):
        raise LetItDieSaveError("Starter research table has an unexpected format.")
    else:
        research["user"] = []

    # Enforce the user's requirement: fighter #1 remains byte-for-byte equal at
    # the JSON-object level; fighter #2/current USE is the one converted.
    fighters_after = get_fighters(root)
    if str(fighters_after[0].get("cid") or "") != first_cid or fighters_after[0] != first_snapshot:
        raise LetItDieSaveError("Safety check failed: the first fighter was modified.")

    return root


# Generates an AP starter save without overwriting the user's vanilla save.
def generate_archipelago_save(
    template_save: str | Path,
    destination_save: str | Path,
    settings: StarterSaveSettings = StarterSaveSettings(),
) -> Path:
    template_save = Path(template_save)
    destination_save = Path(destination_save)

    if template_save.resolve() == destination_save.resolve():
        raise ValueError("Refusing to overwrite the vanilla template save.")

    source = read_brg_save(template_save)
    root = build_archipelago_start(source.root, settings)
    destination_save.parent.mkdir(parents=True, exist_ok=True)

    return write_brg_save(root, destination_save, backup_existing=True)
