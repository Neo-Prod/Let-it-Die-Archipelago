"""Possible Wednesday placed chests; never enemy/skill loot.

Derived from Research/Indexes/Chests_R68/natural_placed_routes.jsonl.
SHA-256: dbc7610ee177b5b907d95ad9cf11639fc384993430b964ef64e5e5e6acba4ea9
Arena witnesses use boss readiness as a conservative logic policy.
Presence is possible, not guaranteed on every visit. No game scans.
"""

WITNESSES = {
    ('Gold', 0): (('3F KAMI-IKEDOI', False), ('4F REGINAKA', False), ('5F OMENO-INARI', False), ('6F MOKA-MAGOME', False), ('8F KO-ONI', False), ('10F OMENO-SOTO', True)),
    ('Gold', 1): (('12F FUGA-DOKORO', False), ('14F FUNE-HASHI', False), ('17F KYUZAKA', False), ('18F TEMO-UMA', False), ('19F CHITOSE-MEI', False), ('20F HACHIYANMA', True)),
    ('Gold', 2): (('22F HIZASHI-GAOKA', False), ('22F KAWABE', False), ('26F HATAKE-GARA', False), ('28F HYAKKOKU', False), ('28F HYAKUDA', False), ('30F NANBOKUCHO', False), ('30F SAIGETSU', True)),
    ('Gold', 3): (('32F KOSHI-YAMA', False), ('32F SHIROKIN', False), ('35F DAINOBA', False), ('35F WAKABA', False), ('38F TAKANOWA', False), ('38F YATAKA-CHO', False), ('40F FUTAGI', True)),
    ('Red', 0): (('1F IKEGARA', False), ('2F IMOKAWA-CHO', False), ('2F WANOKI', False), ('3F KAMI-IKEDOI', False), ('3F TAMATA', False), ('4F TAGAHARA', False), ('5F HYAKUDORI', False), ('5F JOTEN-JIMA', False), ('6F MEKAROKU-GO', False), ('6F MOKA-MAGOME', False), ('7F DANEDA', False), ('7F YUKI', False), ('8F META-KOYA', False), ('8F NIKO-SENZOKU', False), ('9F NASHIYOGUCHI', False), ('9F YOSHIKI-DANI', False), ('10F OMENO-SOTO', True), ('10F SHIMO-OMENO', False)),
    ('Red', 1): (('11F MAKA-TSUTSUMI', False), ('11F NOKEJIRI', False), ('12F SEYANE', False), ('12F UKONGAOKA', False), ('13F KAMOTO', False), ('13F MOMOHARA', False), ('14F KOSHI-TAGAWA', False), ('14F YAMADAI', False), ('15F UGE', False), ('15F USOGAYA', False), ('16F KAGA', False), ('17F KINUTAMIN', False), ('18F TATA', False), ('18F TEMO-UMA', False), ('19F CHITOSE-MEI', False), ('19F DAIDA', False), ('19F HIBIKI', False), ('20F HACHIYANMA', True), ('20F MONEGI', False), ('20F NORA', False)),
    ('Red', 2): (('21F AKAMI', False), ('21F ARIGURE', False), ('22F IWAJIMA', False), ('22F KAWABE', False), ('23F ECHIZEN-JIMA', False), ('23F HONKAWA', False), ('23F SENSU-BASHI', False), ('24F AGASU', False), ('24F MORIBA', False), ('24F SUMIKIYO', False), ('25F KANIE', False), ('25F OSHI-HAMA', False), ('26F OSHIMI', False), ('26F SHIMEMENO', False), ('27F KUROKAWA', False), ('27F SHINKO-BASHI', False), ('27F YUKIYOSHI', False), ('28F HYAKKOKU', False), ('28F HYAKUDA', False), ('29F HASHIDAKA', False), ('30F SAIGETSU', True), ('30F TOWADA', False)),
    ('Red', 3): (('31F KOI', False), ('31F OBARA', False), ('32F KATSUMA', False), ('32F KOSHI-YAMA', False), ('32F OSUKI', False), ('33F AKABADAI', False), ('33F TOGOSE', False), ('33F UCHINOBU', False), ('34F NITANDA', False), ('34F OTANI-YAMA', False), ('35F DAINOBA', False), ('35F SHINA-YAMA', False), ('35F WAKABA', False), ('36F HIRANUMA', False), ('36F ITANODAI', False), ('36F MOMONOKI-ZAKA', False), ('36F YATSUO', False), ('37F HIROMATSU', False), ('37F MITSUBA', False), ('37F MOGURO', False), ('38F EBATA', False), ('38F KOBA', False), ('38F TAKANOWA', False), ('38F YATAKA-CHO', False), ('39F HARATSUKA', False), ('39F NAKASAKI', False), ('40F FUTAGI', True)),
    ('Silver', 0): (('1F IKEGARA', False), ('2F WANOKI', False), ('3F TAMATA', False), ('4F TAGAHARA', False), ('5F HYAKUDORI', False), ('5F JOTEN-JIMA', False), ('6F MEKAROKU-GO', False), ('6F MOKA-MAGOME', False), ('7F DANEDA', False), ('7F YUKI', False), ('8F NIKO-SENZOKU', False), ('9F YOSHIKI-DANI', False), ('10F OMENO-SOTO', True), ('10F SHIMO-OMENO', False)),
    ('Silver', 1): (('11F MAKA-TSUTSUMI', False), ('12F SEYANE', False), ('13F KAMOTO', False), ('13F MOMOHARA', False), ('16F KAGA', False), ('17F KINUTAMIN', False), ('18F TATA', False), ('18F TEMO-UMA', False), ('19F CHITOSE-MEI', False), ('19F DAIDA', False), ('19F HIBIKI', False), ('20F HACHIYANMA', True), ('20F MONEGI', False), ('20F NORA', False)),
    ('Silver', 2): (('22F IWAJIMA', False), ('23F ECHIZEN-JIMA', False), ('23F HONKAWA', False), ('25F OSHI-HAMA', False), ('26F SHIMEMENO', False), ('27F KUROKAWA', False), ('27F SHINKO-BASHI', False), ('28F HYAKKOKU', False), ('28F HYAKUDA', False), ('29F HASHIDAKA', False), ('30F SAIGETSU', True)),
    ('Silver', 3): (('31F OBARA', False), ('32F KATSUMA', False), ('32F KOSHI-YAMA', False), ('32F OSUKI', False), ('33F AKABADAI', False), ('33F TOGOSE', False), ('33F UCHINOBU', False), ('34F NITANDA', False), ('34F OTANI-YAMA', False), ('35F SHINA-YAMA', False), ('35F WAKABA', False), ('36F HIRANUMA', False), ('36F ITANODAI', False), ('36F MOMONOKI-ZAKA', False), ('36F YATSUO', False), ('37F HIROMATSU', False), ('37F MITSUBA', False), ('37F MOGURO', False), ('38F EBATA', False), ('38F KOBA', False), ('38F YATAKA-CHO', False), ('39F HARATSUKA', False), ('39F NAKASAKI', False), ('40F FUTAGI', True)),
}


def witness_rooms(color, zone, goal):
    return tuple((room, boss) for room, boss in WITNESSES.get((color, zone), ())
                 if int(room.split('F', 1)[0]) <= goal)
