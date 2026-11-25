"""Safe placement requirements and exact current AP weapon requirements for UT."""
from . import Items,EquipmentProgression,MasteryCatalogue

STATS={'HP':('hp','HP'),'STM':('stm','STM'),'STR':('str','STR'),'DEX':('dex','DEX'),
       'VIT':('vit','VIT'),'LUK':('luck','Luck')}

def requirement(level,base,coefficient):
    """Native CalcPartParam140fcdd40 integer percent arithmetic."""
    if not 1<=level<=25 or base<0 or not 0<=coefficient<=1000:raise ValueError('Equipment requirement input')
    if level<2:return base
    if level==2:return base*coefficient//100
    value=coefficient;iterations=level-2
    for i in range(iterations):
        value*=coefficient
        if i>4:value//=100
    if iterations<=4:value*=100**(5-iterations)
    return base*value//1000000000000

class MasteryLogic:
    def __init__(self,world):
        self.player=world.player;self.options=world.options;self.alternatives={f:[]for f in MasteryCatalogue.data()['families']}
        self.current_stages={}
        passthrough=getattr(getattr(world,'multiworld',None),'re_gen_passthrough',{})
        self.tracker_mode=getattr(world,'game','LET IT DIE')in passthrough
        for item,quantity in Items.get_equipment_quantities(world.options).items():
            if not quantity or item not in MasteryCatalogue.data()['weapons']:continue
            stages=EquipmentProgression.stages_for(item,EquipmentProgression.option_steps(world.options))[:quantity]
            current=[];by_part={}
            for copies,stage in enumerate(stages,1):
                row=MasteryCatalogue.data()['weapons'][item][stage.part]
                threshold=requirement(stage.native_level,row['base'],row['coefficient'])
                current.append((stage.part,row['family'],row['stat'],threshold))
                first,maximum=by_part.get(stage.part,(copies,threshold))
                by_part[stage.part]=(first,max(maximum,threshold))
            self.current_stages[item]=tuple(current)
            # Placement must remain safe even when several upgrades arrive in
            # one Waiting Room batch, before a weaker version can be purchased.
            for part,(first,maximum)in by_part.items():
                row=MasteryCatalogue.data()['weapons'][item][part]
                self.alternatives[row['family']].append((item,first,row['stat'],maximum))
    def stat(self,state,stat):
        key,label=STATS[stat];start='luk'if key=='luck'else key
        value=lambda n:int(getattr(self.options,n).value)
        return value('starting_'+start)+min(state.count('Progressive '+label,self.player),value('progressive_'+key+'_quantity'))*value('progressive_'+key+'_increase')
    def can_earn(self,state,family):
        if self.tracker_mode:return self.can_earn_current(state,family)
        return self.can_earn_safely(state,family)
    def can_earn_safely(self,state,family):
        if family=='PTARMTP_00':return True
        return any(state.has(item,self.player,copies)and self.stat(state,stat)>=threshold
                   for item,copies,stat,threshold in self.alternatives[family])
    def can_earn_current(self,state,family):
        if family=='PTARMTP_00':return True
        for item,stages in self.current_stages.items():
            copies=max(0,min(state.count(item,self.player),len(stages)));parts={}
            for part,actual,stat,threshold in stages[:copies]:parts[part]=(actual,stat,threshold)
            if any(actual==family and self.stat(state,stat)>=threshold for actual,stat,threshold in parts.values()):return True
        return False
    def validate_pool(self,policy):
        if policy is None or not policy['enabled']:return
        quantities=Items.get_equipment_quantities(self.options)
        for key,label in STATS.values():quantities['Progressive '+label]=int(getattr(self.options,'progressive_'+key+'_quantity').value)
        class FullPool:
            def count(self,name,player):return quantities.get(name,0)
            def has(self,name,player,count=1):return self.count(name,player)>=count
        blocked=[MasteryCatalogue.data()['families'][f]['name']for f in policy['families']if not self.can_earn_safely(FullPool(),f)]
        if blocked:raise ValueError('MasterySanity: configured maximum character stats cannot equip any AP weapon for: '+', '.join(blocked)+'. Increase starting stats or stat progressive gains/quantities, or exclude those weapons from the pool.')
