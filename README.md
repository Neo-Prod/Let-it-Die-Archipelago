# LET IT DIE Archipelago - Version 1.3

## Progressive Floor

**progressive_floor** is optional and defaults to false. When enabled, Progressive Floor permanently unlocks numbered ascents within each ten-floor band. The transitions10->11,20->21,30->31 and40->41 are exempt. **progressive_floor_quantity** defaults to36. Minimum quantities by goal are9 for Max Sharps,18 for Jackson,27 for Crowley and36 for Taro Gunkanyama. Insufficient enabled quantities are rejected during generation. Floor items apply through the existing Waiting Room delivery point; extra copies beyond the goal requirement have no further effect. Universal Tracker uses the same floor requirements.

## MasterySanity

- **mastery_sanity**: optional mastery checks, false by default.
- **mastery_checks**:1..20 checks per eligible mastery, default 20. Levels are evenly spaced and always include 20. Seven checks use3,6,9,11,14,17,20.
- **mastery_include_fists**: include fist checks, true by default. Fists also cap at 20.
- **mastery_xp_multiplier**:1..100, default 1. Multiplies native mastery XP from enemy defeats; normal fighter XP is unchanged. This option also works with MasterySanity disabled, I recommend multiplying the XP by 10 if you activate mastery sanity

Only weapon families available through the run's allowed AP progressives receive mastery locations. The tracker also considers received weapons, their current upgrade requirements and the six AP base stats. Optional temporary bonuses or requirement-reducing decals are not assumed as guaranteed access. Native level rewards and XP display remain in control of the game.

## Shop Sanity

With ShopSanity enabled, opening a buy list reveals Archipelago hints for that merchant's currently available, unpurchased offers. Momoka hints only her currently unlocked Kill Coin offers, including all available categories; later unlocks are not revealed in advance. Insufficient KC does not hide an otherwise unlocked offer. Tower hints belong to the exact merchant opened.

Each shop location is hinted once per run. This addition needs no new generation for an existing ShopSanity-enabled run. Opening a shop does not buy an item or grant a reward.

## Bug fix

There was an issue in previous versions where your character's stats were the same as the rank 1 screamers, so the higher your stats were, the higher the enemies' stats were too. The problem has been fixed.
