"""LET IT DIE Archipelago world."""

from typing import Any, Mapping

from BaseClasses import Tutorial, ItemClassification
from worlds.AutoWorld import WebWorld, World

from . import Items, Locations, Regions, Rules
from .Items import LetItDieItem
from .Locations import LetItDieLocation
from .Options import LetItDieOptions
from .TowerLogic import TowerLogic, restore_tracker_options, tracker_slot_data
from .Fillers import choose_filler, configured_weights


class LetItDieWebWorld(WebWorld):
    """WebHost metadata for LET IT DIE."""

    theme = "stone"
    rich_text_options_doc = True

    setup_en = Tutorial(
        "Multiworld Setup Guide",
        "A guide to setting up LET IT DIE Offline Edition for Archipelago.",
        "English",
        "setup_en.md",
        "setup/en",
        ["LET IT DIE Archipelago project"],
    )

    tutorials = [setup_en]


class LetItDieWorld(World):
    """Archipelago implementation for LET IT DIE Offline Edition."""

    game = "LET IT DIE"
    web = LetItDieWebWorld()

    origin_region_name = "Waiting Room"
    ut_can_gen_without_yaml = True
    hater_catalogue_version = 5
    generation_schema = 2

    @staticmethod
    def interpret_slot_data(slot_data: dict[str, Any]) -> dict[str, Any]:
        """Ask UT to regenerate using this run's exact gameplay options."""
        return slot_data

    options_dataclass = LetItDieOptions
    options: LetItDieOptions

    item_name_to_id = Items.item_name_to_id
    location_name_to_id = Locations.location_name_to_id
    item_name_groups = Items.ITEM_NAME_GROUPS
    location_name_groups = Locations.LOCATION_NAME_GROUPS

    item_type = LetItDieItem
    location_type = LetItDieLocation

    generated_item_count: int = 0

    def generate_early(self) -> None:
        """Calculate the item count before dynamic Screamer locations are created."""

        restore_tracker_options(self)
        if self.generation_schema >= 2:
            configured_weights(self.options)
        self.generated_item_count = (
            int(self.options.progressive_hp_quantity.value)
            + int(self.options.progressive_stm_quantity.value)
            + int(self.options.progressive_str_quantity.value)
            + int(self.options.progressive_dex_quantity.value)
            + int(self.options.progressive_vit_quantity.value)
            + int(self.options.progressive_luck_quantity.value)
            + int(self.options.progressive_death_bag_quantity.value)
            + int(self.options.progressive_skill_slots_quantity.value)
            + int(self.options.progressive_rage_gauge_quantity.value)
            + sum(Items.get_equipment_quantities(self.options).values())
            + len(Items.PREMIUM_DECAL_ITEMS)
        )

        self.check_catalogue = Locations.build_check_catalogue(self)
        self.tower_logic = TowerLogic(self)
        self.tower_logic.validate_pool()

    def create_regions(self) -> None:
        """Create regions and the exact number of randomized locations."""

        Regions.create_regions(self)

    def create_item(self, name: str) -> LetItDieItem:
        """Create one LET IT DIE item."""

        data = Items.item_table[name]
        return LetItDieItem(
            name,
            (ItemClassification.progression_skip_balancing
             if data.category in ('stat', 'equipment', 'armor_set') else data.classification),
            data.code,
            self.player,
        )

    def create_items(self) -> None:
        """Create configured progressive items, equipment, decals and filler."""

        pool = []

        configured_quantities = {
            "Progressive HP": int(self.options.progressive_hp_quantity.value),
            "Progressive STM": int(self.options.progressive_stm_quantity.value),
            "Progressive STR": int(self.options.progressive_str_quantity.value),
            "Progressive DEX": int(self.options.progressive_dex_quantity.value),
            "Progressive VIT": int(self.options.progressive_vit_quantity.value),
            "Progressive Luck": int(self.options.progressive_luck_quantity.value),
            "Progressive Death Bag": int(self.options.progressive_death_bag_quantity.value),
            "Progressive Skill Slots": int(self.options.progressive_skill_slots_quantity.value),
            "Progressive Rage Gauge": int(self.options.progressive_rage_gauge_quantity.value),
        }

        for name, quantity in configured_quantities.items():
            pool.extend(self.create_item(name) for _ in range(quantity))

        for name, quantity in Items.get_equipment_quantities(self.options).items():
            pool.extend(self.create_item(name) for _ in range(quantity))

        for name in Items.PREMIUM_DECAL_ITEMS:
            pool.append(self.create_item(name))

        randomized_location_count = len(self.check_catalogue['checks'])

        while len(pool) < randomized_location_count:
            pool.append(self.create_item(self.get_filler_item_name()))

        if len(pool) != randomized_location_count:
            raise ValueError(
                "LET IT DIE item/location mismatch: "
                f"{len(pool)} items for {randomized_location_count} locations."
            )

        self.multiworld.itempool += pool

    def set_rules(self) -> None:
        """Set the selected boss completion condition."""

        Rules.set_rules(self)

    def get_filler_item_name(self) -> str:
        """Return the repeatable filler item."""

        return choose_filler(self)

    def fill_slot_data(self) -> Mapping[str, Any]:
        """Send client-side gameplay parameters with the generated slot."""

        return {
            "tracker": tracker_slot_data(self),
            "enable_dod_arms": bool(self.options.enable_dod_arms.value),
            "enable_war_ensemble": bool(self.options.enable_war_ensemble.value),
            "enable_candle_wolf": bool(self.options.enable_candle_wolf.value),
            "enable_milk": bool(self.options.enable_milk.value),
            "maximum_equipment_tier": int(self.options.maximum_equipment_tier.value),
            "group_armor_sets": bool(self.options.group_armor_sets.value),
            "starting_fighter_class": self.options.starting_fighter_class.current_key,
            "starting_fighter_grade": int(self.options.starting_fighter_grade.value),

            "starting_hp": int(self.options.starting_hp.value),
            "starting_stm": int(self.options.starting_stm.value),
            "starting_str": int(self.options.starting_str.value),
            "starting_dex": int(self.options.starting_dex.value),
            "starting_vit": int(self.options.starting_vit.value),
            "starting_luk": int(self.options.starting_luk.value),
            "starting_skill_slots": int(self.options.starting_skill_slots.value),
            "starting_death_bag": int(self.options.starting_death_bag.value),
            "starting_rage_gauge": int(self.options.starting_rage_gauge.value),

            "progressive_hp_increase": int(self.options.progressive_hp_increase.value),
            "progressive_stm_increase": int(self.options.progressive_stm_increase.value),
            "progressive_str_increase": int(self.options.progressive_str_increase.value),
            "progressive_dex_increase": int(self.options.progressive_dex_increase.value),
            "progressive_vit_increase": int(self.options.progressive_vit_increase.value),
            "progressive_luck_increase": int(self.options.progressive_luck_increase.value),
            "progressive_death_bag_increase": int(self.options.progressive_death_bag_increase.value),
            "progressive_skill_slots_increase": int(self.options.progressive_skill_slots_increase.value),
            "progressive_rage_gauge_increase": int(self.options.progressive_rage_gauge_increase.value),

            # V0.74: quantities are runtime data too. The native game has
            # Grade-6 rows through level 45, so starting at level 1 allows at
            # most 44 progressive increases for any one progression track.
            "progressive_hp_quantity": int(self.options.progressive_hp_quantity.value),
            "progressive_stm_quantity": int(self.options.progressive_stm_quantity.value),
            "progressive_str_quantity": int(self.options.progressive_str_quantity.value),
            "progressive_dex_quantity": int(self.options.progressive_dex_quantity.value),
            "progressive_vit_quantity": int(self.options.progressive_vit_quantity.value),
            "progressive_luck_quantity": int(self.options.progressive_luck_quantity.value),
            "progressive_death_bag_quantity": int(self.options.progressive_death_bag_quantity.value),
            "progressive_skill_slots_quantity": int(self.options.progressive_skill_slots_quantity.value),
            "progressive_rage_gauge_quantity": int(self.options.progressive_rage_gauge_quantity.value),

            "screamer_check_chance": int(self.options.screamer_check_chance.value),
            "screamer_pity": bool(self.options.screamer_pity.value),

            "fixed_location_count": self.check_catalogue['fixed_count'],
            "generated_item_count": self.generated_item_count,
            "screamer_location_count": sum(self.check_catalogue['screamer_counts']),
            "goal": int(self.options.goal.value),
            "checks": {
                "schema": 2,
                "required_rotation": "4HMA",
                "goal_floor": self.check_catalogue['goal_floor'],
                "goal_name": self.check_catalogue['goal_name'],
                "tuber_checks_per_version": {
                    family: int(getattr(self.options, family.lower() + '_checks').value)
                    for family in ("Scratch", "Hovering", "Bullet", "Bone", "Reversal", "Treasure")
                },
                **({"chest_checks_per_band": {color: int(getattr(self.options, color.lower() + "_chest_checks").value)
                      for color in ("Red", "Silver", "Gold")}} if self.hater_catalogue_version >= 4 else {}),
                **({"material_checks_per_type": {"normal": int(self.options.material_checks.value),
                                                  "rare": int(self.options.rare_material_checks.value),
                                                  "metal": int(self.options.faction_metal_checks.value)}} if self.hater_catalogue_version >= 5 else {}),
                "locations": self.check_catalogue['checks'],
                **({"minimum_screamer_checks_per_band": int(self.options.minimum_screamer_checks_per_band.value)}
                   if self.generation_schema >= 2 else {}),
            },
        }

# Registers the external LET IT DIE client in Archipelago Launcher. Networking
# stays inside Archipelago's own CommonClient stack; the game DLL only hands it
# a connection request.
def _register_let_it_die_client() -> None:
    try:
        from worlds.LauncherComponents import Component, Type, components, launch, SuffixIdentifier
    except Exception:
        return

    # Remove stale LET IT DIE client registrations from older copies of this
    # APWorld so .lidapconnect always resolves to the newest loaded client.
    components[:] = [
        component for component in components
        if not (
            str(getattr(component, "display_name", "")).startswith("LET IT DIE Client V0.")
            or (
                getattr(component, "game_name", None) == "LET IT DIE"
                and str(getattr(component, "display_name", "")).startswith("LET IT DIE Client")
            )
        )
    ]

    # Use Archipelago's normal launch() dispatcher and .lidapconnect file association.
    # When the Launcher
    # is invoked from the native DLL by component name, there is no Kivy GUI,
    # so launch() calls the client directly instead of forcing a second
    # multiprocessing spawn.  This keeps custom-APWorld imports in the process
    # that already loaded this world.
    def _launch(*args: str) -> None:
        try:
            # V0.52: marker written before importing Client.py. This proves that
            # Archipelago resolved the .lidapconnect request to this exact world.
            if args:
                from pathlib import Path
                request_path = Path(args[0]).resolve()
                bridge_dir = request_path.parent
                bridge_dir.mkdir(parents=True, exist_ok=True)
                with (bridge_dir / "component_entry.log").open("a", encoding="utf-8") as log:
                    log.write(f"V0.52 component invoked; request={request_path}\n")
            from .Client import launch_from_archipelago
            launch(launch_from_archipelago, name="LET IT DIE Client V0.52", args=args)
        except Exception as exc:
            # The native side is waiting for connection_result.json.  Guarantee
            # a result even if Client.py itself fails to import or initialize.
            if args:
                try:
                    import json
                    from pathlib import Path
                    request_path = Path(args[0]).resolve()
                    bridge_dir = request_path.parent
                    bridge_dir.mkdir(parents=True, exist_ok=True)
                    message = f"LET IT DIE client bootstrap failed: {type(exc).__name__}: {exc}"
                    result_path = bridge_dir / "connection_result.json"
                    temp_path = result_path.with_suffix(".tmp")
                    temp_path.write_text(
                        json.dumps({"status": "error", "message": message}, ensure_ascii=False, indent=2),
                        encoding="utf-8",
                    )
                    temp_path.replace(result_path)
                    with (bridge_dir / "client_bootstrap.log").open("a", encoding="utf-8") as log:
                        log.write(message + "\n")
                except Exception:
                    pass

    components.append(
        Component(
            "LET IT DIE Client V0.52",
            func=_launch,
            component_type=Type.CLIENT,
            file_identifier=SuffixIdentifier(".lidapconnect"),
            game_name="LET IT DIE",
            supports_uri=False,
            description="Connect LET IT DIE Offline Edition to an Archipelago session (V0.52 hot-slot bridge).",
        )
    )


_register_let_it_die_client()
