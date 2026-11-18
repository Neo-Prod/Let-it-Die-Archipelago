"""Frozen catalogue provenance is distinct from the active executable build.

The eight source master tables are unchanged between these two exact builds
(Research/Tests/Compatibility_R312/catalogue_source_compatibility.json).
Keep original JSON provenance/digests; never relabel old evidence as new.
Live protocol and executable checks still require the current runtime SHA.
"""
from .CheckWire import GAME

SOURCE_GAME='f33e91e6e8d58566bfabad8d01cd0486586a0d43fe1c58e65fc6485a417bddfd'
CURRENT_GAME='6cfadbba9da3e2f3b3a4463194d247a50d2537a3ad704174730a0bbf899e42ca'

def catalogue_build_valid(data):
    return (type(data) is dict and type(data.get('schema')) is int
        and data['schema']==1 and data.get('game_sha256')==SOURCE_GAME
        and GAME.hex()==CURRENT_GAME)
