"""KC filler definitions and deterministic generation weights.

Legacy Kill Coins remains reserved; new denomination IDs never reuse its ID.
This module does not grant currency or access the game.
"""
KILL_COIN_FILLERS = (
    ("2,000 Kill Coins", 7130002, 2000, "kill_coins_2000_percentage", 70),
    ("10,000 Kill Coins", 7130003, 10000, "kill_coins_10000_percentage", 25),
    ("50,000 Kill Coins", 7130004, 50000, "kill_coins_50000_percentage", 5),
)
AMOUNTS_BY_ID = {code: amount for _, code, amount, _, _ in KILL_COIN_FILLERS}
NEW_OPTION_NAMES = frozenset(("minimum_screamer_checks_per_band", *(row[3] for row in KILL_COIN_FILLERS)))


def configured_weights(options):
    values = tuple(getattr(options, row[3]).value for row in KILL_COIN_FILLERS)
    if any(type(value) is not int or not 0 <= value <= 100 for value in values) or sum(values) != 100:
        raise ValueError("LET IT DIE KC filler percentages must be integers from 0 to 100 and add up to 100.")
    return values


def choose_filler(world):
    if getattr(world, "generation_schema", 2) < 2:
        return "Kill Coins"
    return world.random.choices([row[0] for row in KILL_COIN_FILLERS],
                                weights=configured_weights(world.options), k=1)[0]
