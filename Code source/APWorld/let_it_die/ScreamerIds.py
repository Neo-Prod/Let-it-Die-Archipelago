"""Stable Screamer addresses, including extra slots for enhanced equipment pools."""

SCREAMER_BASE = 7206000
LEGACY_CAP = 2500
SCREAMER_CAP = 10000
OVERFLOW_BASE = 7220000


def screamer_code(zone, ordinal):
    if type(zone) is not int or not 0 <= zone <= 3:
        raise ValueError('Screamer zone')
    if type(ordinal) is not int or not 1 <= ordinal <= SCREAMER_CAP:
        raise ValueError('Screamer ordinal')
    if ordinal <= LEGACY_CAP:
        return SCREAMER_BASE + zone * LEGACY_CAP + ordinal
    return OVERFLOW_BASE + zone * SCREAMER_CAP + ordinal - LEGACY_CAP
