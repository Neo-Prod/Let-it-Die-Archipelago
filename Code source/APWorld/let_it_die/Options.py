"""Archipelago options for LET IT DIE."""

from dataclasses import dataclass

from Options import Choice, DefaultOnToggle, PerGameCommonOptions, Range, Toggle


class StartingFighterClass(Choice):
    """Fighter class used for the starting fighter."""

    display_name = "Starting Fighter Class"

    option_all_rounder = 0
    option_striker = 1
    option_defender = 2
    option_attacker = 3
    option_shooter = 4
    option_collector = 5
    option_skill_master = 6
    option_lucky_star = 7

    default = 0


class StartingFighterGrade(Range):
    """Grade used for the starting fighter."""

    display_name = "Starting Fighter Grade"
    range_start = 1
    range_end = 9
    default = 6


class StartingHP(Range):
    """Base HP of the starting fighter before Archipelago items are received."""

    display_name = "Starting HP"
    range_start = 1
    range_end = 100000
    default = 200


class StartingSTM(Range):
    """Base stamina of the starting fighter."""

    display_name = "Starting STM"
    range_start = 1
    range_end = 10000
    default = 200


class StartingSTR(Range):
    """Base strength of the starting fighter."""

    display_name = "Starting STR"
    range_start = 0
    range_end = 10000
    default = 10


class StartingDEX(Range):
    """Base dexterity of the starting fighter."""

    display_name = "Starting DEX"
    range_start = 0
    range_end = 10000
    default = 10


class StartingVIT(Range):
    """Base vitality of the starting fighter."""

    display_name = "Starting VIT"
    range_start = 0
    range_end = 10000
    default = 10


class StartingLUK(Range):
    """Base luck of the starting fighter."""

    display_name = "Starting LUK"
    range_start = 0
    range_end = 10000
    default = 10


class StartingSkillSlots(Range):
    """Base number of skill/decal slots."""

    display_name = "Starting Skill Slots"
    range_start = 0
    range_end = 20
    default = 0


class StartingDeathBag(Range):
    """Base Death Bag capacity."""

    display_name = "Starting Death Bag"
    range_start = 1
    range_end = 100
    default = 1


class StartingRageGauge(Range):
    """Base Rage Gauge capacity."""

    display_name = "Starting Rage Gauge"
    range_start = 0
    range_end = 20
    default = 0


# ---------------------------------------------------------------------------
# Progressive item quantities
# ---------------------------------------------------------------------------

class ProgressiveHPQuantity(Range):
    """Number of Progressive HP items placed in the item pool."""

    display_name = "Progressive HP Quantity"
    range_start = 0
    range_end = 44
    default = 30


class ProgressiveSTMQuantity(Range):
    """Number of Progressive STM items placed in the item pool."""

    display_name = "Progressive STM Quantity"
    range_start = 0
    range_end = 44
    default = 30


class ProgressiveSTRQuantity(Range):
    """Number of Progressive STR items placed in the item pool."""

    display_name = "Progressive STR Quantity"
    range_start = 0
    range_end = 44
    default = 30


class ProgressiveDEXQuantity(Range):
    """Number of Progressive DEX items placed in the item pool."""

    display_name = "Progressive DEX Quantity"
    range_start = 0
    range_end = 44
    default = 30


class ProgressiveVITQuantity(Range):
    """Number of Progressive VIT items placed in the item pool."""

    display_name = "Progressive VIT Quantity"
    range_start = 0
    range_end = 44
    default = 30


class ProgressiveLuckQuantity(Range):
    """Number of Progressive Luck items placed in the item pool."""

    display_name = "Progressive Luck Quantity"
    range_start = 0
    range_end = 44
    default = 30


class ProgressiveDeathBagQuantity(Range):
    """Number of Progressive Death Bag items placed in the item pool."""

    display_name = "Progressive Death Bag Quantity"
    range_start = 0
    range_end = 44
    default = 30


class ProgressiveSkillSlotsQuantity(Range):
    """Number of Progressive Skill Slots items placed in the item pool."""

    display_name = "Progressive Skill Slots Quantity"
    range_start = 0
    range_end = 44
    default = 9


class ProgressiveRageGaugeQuantity(Range):
    """Number of Progressive Rage Gauge items placed in the item pool."""

    display_name = "Progressive Rage Gauge Quantity"
    range_start = 0
    range_end = 44
    default = 6


# ---------------------------------------------------------------------------
# Progressive stat gains
# ---------------------------------------------------------------------------

class ProgressiveHPIncrease(Range):
    """HP gained for each Progressive HP item."""

    display_name = "Progressive HP Increase"
    range_start = 0
    range_end = 100000
    default = 200


class ProgressiveSTMIncrease(Range):
    """STM gained for each Progressive STM item."""

    display_name = "Progressive STM Increase"
    range_start = 0
    range_end = 10000
    default = 5


class ProgressiveSTRIncrease(Range):
    """STR gained for each Progressive STR item."""

    display_name = "Progressive STR Increase"
    range_start = 0
    range_end = 10000
    default = 6


class ProgressiveDEXIncrease(Range):
    """DEX gained for each Progressive DEX item."""

    display_name = "Progressive DEX Increase"
    range_start = 0
    range_end = 10000
    default = 6


class ProgressiveVITIncrease(Range):
    """VIT gained for each Progressive VIT item."""

    display_name = "Progressive VIT Increase"
    range_start = 0
    range_end = 10000
    default = 3


class ProgressiveLuckIncrease(Range):
    """Luck gained for each Progressive Luck item."""

    display_name = "Progressive Luck Increase"
    range_start = 0
    range_end = 10000
    default = 2


class ProgressiveDeathBagIncrease(Range):
    """Death Bag capacity gained per Progressive Death Bag item."""

    display_name = "Progressive Death Bag Increase"
    range_start = 0
    range_end = 100
    default = 1


class ProgressiveSkillSlotsIncrease(Range):
    """Skill/decal slots gained per Progressive Skill Slots item."""

    display_name = "Progressive Skill Slots Increase"
    range_start = 0
    range_end = 20
    default = 1


class ProgressiveRageGaugeIncrease(Range):
    """Rage Gauge capacity gained per Progressive Rage Gauge item."""

    display_name = "Progressive Rage Gauge Increase"
    range_start = 0
    range_end = 20
    default = 1


class ScreamerCheckChance(Range):
    """Percent chance for an eligible Screamer kill to complete the next
    pending Screamer location in the current 10-floor zone.
    """

    display_name = "Screamer Check Chance"
    range_start = 1
    range_end = 100
    default = 25


class EnableDODArms(DefaultOnToggle):
    """Include D.O.D. Arms weapons and head/body/leg armor in the generated pool."""

    display_name = "Enable D.O.D. Arms"


class EnableWarEnsemble(DefaultOnToggle):
    """Include War Ensemble weapons and head/body/leg armor in the generated pool."""

    display_name = "Enable War Ensemble"


class EnableCandleWolf(DefaultOnToggle):
    """Include Candle Wolf weapons and head/body/leg armor in the generated pool."""

    display_name = "Enable Candle Wolf"


class EnableMILK(DefaultOnToggle):
    """Include M.I.L.K. weapons and head/body/leg armor in the generated pool."""

    display_name = "Enable M.I.L.K."


class GroupArmorSets(Toggle):
    """Replace each complete head/body/legs armor set's three progressive
    families with one progressive. Each receipt grants all three blueprints
    of the next step. A four-step set needs four receipts instead of twelve.
    Isolated pieces and weapons remain separate. Default: disabled.
    """

    display_name = "Group Armor Sets"


class MaximumEquipmentTier(Range):
    """Maximum global equipment tier from the user tier tables (1 to 6).

    Each equipment remains a distinct progressive step. Two equipment types
    in the same tier both keep their own receipt. This is not the number of
    progressive copies per family, nor the item's star rarity.
    """

    display_name = "Maximum Equipment Tier"
    range_start = 1
    range_end = 6
    default = 6


class Goal(Choice):
    """Final boss. Checks above this boss's floor are excluded."""
    display_name = "Goal"
    option_max_sharp = 10
    option_jackson = 20
    option_crowley = 30
    option_taro_gunkanyama = 40
    default = 40


class TuberChecks(Range):
    """Checks per eligible V-version of this family; one kill advances one check."""
    range_start = 0
    range_end = 1000


class ScratchChecks(TuberChecks):
    display_name = "Scratch Checks Per Version"
    default = 20


class HoveringChecks(TuberChecks):
    display_name = "Hovering Checks Per Version"
    default = 5


class BulletChecks(TuberChecks):
    display_name = "Bullet Checks Per Version"
    default = 10


class BoneChecks(TuberChecks):
    display_name = "Bone Checks Per Version"
    default = 3


class ReversalChecks(TuberChecks):
    display_name = "Reversal Checks Per Version"
    default = 1


class TreasureChecks(TuberChecks):
    display_name = "Treasure Checks Per Version"
    default = 1


class MinimumScreamerChecksPerBand(Range):
    """Minimum number of Screamer check locations in each ten-floor band.
    More checks are added when needed to place all configured items.
    Extra locations are filled with weighted KC fillers. The existing
    Screamer check chance and pity still determine how many kills are needed.
    """
    display_name = "Minimum Screamer Checks Per Ten Floors"
    range_start = 0
    range_end = 2500
    default = 100


class KillCoins2000Percentage(Range):
    """Chance for a filler location to contain 2,000 KC.
    All three KC filler percentages must add up to 100.
    """
    display_name = "2,000 KC Filler Percentage"
    range_start = 0
    range_end = 100
    default = 70


class KillCoins10000Percentage(KillCoins2000Percentage):
    """Chance for a filler location to contain 10,000 KC.
    All three KC filler percentages must add up to 100.
    """
    display_name = "10,000 KC Filler Percentage"
    default = 25


class KillCoins50000Percentage(KillCoins2000Percentage):
    """Chance for a filler location to contain 50,000 KC.
    All three KC filler percentages must add up to 100.
    """
    display_name = "50,000 KC Filler Percentage"
    default = 5


class ScreamerPity(Toggle):
    """Guarantee a Screamer check after ceil(100 / screamer_check_chance)
    eligible kills without a check. Any check resets the counter.
    Counters persist separately for each ten-floor band. Default: disabled.
    """
    display_name = "Screamer Pity"


class ChestChecks(Range):
    """Chest opening checks per ten-floor band, bounded by the goal. Zero disables the color."""
    range_start = 0
    range_end = 1000


class RedChestChecks(ChestChecks):
    display_name = "Red Chest Checks Per Band"
    default = 30


class SilverChestChecks(ChestChecks):
    display_name = "Silver Chest Checks Per Band"
    default = 15


class GoldChestChecks(ChestChecks):
    display_name = "Gold Chest Checks Per Band"
    default = 3


class MaterialChecks(Range):
    """Checks per ordinary material type and star rank. Cloth ranks 1-5 use this count.
    Set to zero to disable these checks. Materials are consumed when picked up.
    """
    display_name = "Material Checks Per Type And Rank"
    range_start = 0
    range_end = 1000
    default = 10


class RareMaterialChecks(Range):
    """Checks per rare material type one star above the selected goal's ordinary ranks.
    Cloth uses material_checks instead. Set to zero to disable rare-rank checks.
    """
    display_name = "Rare Material Checks Per Type"
    range_start = 0
    range_end = 1000
    default = 2


class FactionMetalChecks(Range):
    """Checks per faction metal type and rank available before the selected goal.
    Faction metal ranks are capped at four stars. Zero disables these checks.
    """
    display_name = "Faction Metal Checks Per Type And Rank"
    range_start = 0
    range_end = 1000
    default = 1


@dataclass
class LetItDieOptions(PerGameCommonOptions):
    """All player-configurable LET IT DIE options."""

    starting_fighter_class: StartingFighterClass
    starting_fighter_grade: StartingFighterGrade

    starting_hp: StartingHP
    starting_stm: StartingSTM
    starting_str: StartingSTR
    starting_dex: StartingDEX
    starting_vit: StartingVIT
    starting_luk: StartingLUK
    starting_skill_slots: StartingSkillSlots
    starting_death_bag: StartingDeathBag
    starting_rage_gauge: StartingRageGauge

    progressive_hp_quantity: ProgressiveHPQuantity
    progressive_stm_quantity: ProgressiveSTMQuantity
    progressive_str_quantity: ProgressiveSTRQuantity
    progressive_dex_quantity: ProgressiveDEXQuantity
    progressive_vit_quantity: ProgressiveVITQuantity
    progressive_luck_quantity: ProgressiveLuckQuantity
    progressive_death_bag_quantity: ProgressiveDeathBagQuantity
    progressive_skill_slots_quantity: ProgressiveSkillSlotsQuantity
    progressive_rage_gauge_quantity: ProgressiveRageGaugeQuantity

    progressive_hp_increase: ProgressiveHPIncrease
    progressive_stm_increase: ProgressiveSTMIncrease
    progressive_str_increase: ProgressiveSTRIncrease
    progressive_dex_increase: ProgressiveDEXIncrease
    progressive_vit_increase: ProgressiveVITIncrease
    progressive_luck_increase: ProgressiveLuckIncrease
    progressive_death_bag_increase: ProgressiveDeathBagIncrease
    progressive_skill_slots_increase: ProgressiveSkillSlotsIncrease
    progressive_rage_gauge_increase: ProgressiveRageGaugeIncrease

    screamer_check_chance: ScreamerCheckChance
    screamer_pity: ScreamerPity
    minimum_screamer_checks_per_band: MinimumScreamerChecksPerBand
    kill_coins_2000_percentage: KillCoins2000Percentage
    kill_coins_10000_percentage: KillCoins10000Percentage
    kill_coins_50000_percentage: KillCoins50000Percentage

    enable_dod_arms: EnableDODArms
    enable_war_ensemble: EnableWarEnsemble
    enable_candle_wolf: EnableCandleWolf
    enable_milk: EnableMILK
    maximum_equipment_tier: MaximumEquipmentTier
    group_armor_sets: GroupArmorSets
    goal: Goal
    scratch_checks: ScratchChecks
    hovering_checks: HoveringChecks
    bullet_checks: BulletChecks
    bone_checks: BoneChecks
    reversal_checks: ReversalChecks
    treasure_checks: TreasureChecks
    red_chest_checks: RedChestChecks
    silver_chest_checks: SilverChestChecks
    gold_chest_checks: GoldChestChecks
    material_checks: MaterialChecks
    rare_material_checks: RareMaterialChecks
    faction_metal_checks: FactionMetalChecks
