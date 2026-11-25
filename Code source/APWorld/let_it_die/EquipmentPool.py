"""Generation-only equipment filtering using the reviewed user PDF.

One entry per existing progressive family, one tier per distinct receipt.
Repeated tier numbers do not merge receipts or change native delivery order.
"""
from .EquipmentProgression import stages_for, option_steps
from functools import lru_cache
import json
import pkgutil

FACTION_OPTIONS = {
    "dod_arms": "enable_dod_arms",
    "war_ensemble": "enable_war_ensemble",
    "candle_wolf": "enable_candle_wolf",
    "milk": "enable_milk",
}

# IDs and native catalogue slots remain reserved for older received histories.
REMOVED_EQUIPMENT_NAMES = frozenset({"Progressive Iron Hammer RG"})


@lru_cache(maxsize=2)
def _catalogue(legacy=False):
    raw = pkgutil.get_data(__package__, "EquipmentTiersLegacy.json" if legacy else "EquipmentTiers.json")
    if raw is None:
        raise ValueError("LET IT DIE equipment tier catalogue is missing.")
    data = json.loads(raw)
    if data.get("schema") != 1 or not isinstance(data.get("equipment"), dict):
        raise ValueError("Unsupported LET IT DIE equipment tier catalogue.")
    return data["equipment"]


def _option_value(options, name, default):
    option = getattr(options, name, None)
    return default if option is None else option.value


def equipment_quantities(equipment_items, options):
    """Count eligible distinct paliers, preserving all existing item IDs/data."""
    legacy = bool(getattr(options, "_legacy_equipment", False))
    catalogue = _catalogue(legacy)
    if set(catalogue) != set(equipment_items):
        raise ValueError("LET IT DIE equipment definitions/tier catalogue mismatch.")
    maximum = int(_option_value(options, "maximum_equipment_tier", 6))
    if not 1 <= maximum <= 9:
        raise ValueError("maximum_equipment_tier must be between 1 and 9.")
    enabled = {f: bool(_option_value(options, option, True)) for f, option in FACTION_OPTIONS.items()}
    quantities = {}
    for name, item in equipment_items.items():
        entry = catalogue[name]
        if entry.get("ap_id") != item.code:
            raise ValueError(f"Equipment AP ID mismatch: {name}")
        if name in REMOVED_EQUIPMENT_NAMES or entry.get("excluded"):
            quantities[name] = 0
            continue
        tiers = entry.get("tiers", [])
        faction = entry.get("faction")
        if (faction not in enabled or entry.get("game_ids") != list(item.game_ids)
                or len(tiers) != item.default_quantity or len(tiers) != len(item.game_ids)
                or any(type(t) is not int or not 1 <= t <= 9 for t in tiers)
                or tiers != sorted(tiers)):
            raise ValueError(f"Invalid equipment faction/tier mapping: {name}")
        # Count EACH receipt, including two different items sharing one tier.
        quantities[name] = (sum(t <= maximum for t in tiers) if legacy else sum(s.tier <= maximum for s in stages_for(name, option_steps(options)))) if enabled[faction] else 0
    return quantities
