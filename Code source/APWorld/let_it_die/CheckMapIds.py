"""Map identifiers preserved verbatim; physical floors derived from block."""
import re
PREFIXES=('MET','ARC','AMS','RFT')
def physical_floor(floor,area):
    if type(floor) is not str or type(area) is not str:raise ValueError('map field type')
    match=re.fullmatch(r'(MET|ARC|AMS|RFT)_FLR_(0[1-9]|10)',floor)
    if not match or not re.fullmatch(match[1]+r'_AREA_V?[0-9]{1,12}',area):raise ValueError('tower map identifiers')
    return PREFIXES.index(match[1])*10+int(match[2])
def area_for_floor(area,floor):
    return type(floor) is int and 1<=floor<=40 and type(area) is str and re.fullmatch(PREFIXES[(floor-1)//10]+r'_AREA_V?[0-9]{1,12}',area) is not None
