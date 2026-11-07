"""Additional preflight for native JsonObject representation, no IO/mutation.

This guards comparison assumptions; it is not the complete gameplay schema.
"""
class NativeJsonCompatibilityError(ValueError):
    pass

def validate_native_compatibility(root):
    todo=[(root,0)];nodes=0
    while todo:
        value,depth=todo.pop();nodes+=1
        if nodes>500000 or depth>128:raise NativeJsonCompatibilityError('JSON comparison complexity limit')
        if type(value) is dict:
            folded=set()
            for key,child in value.items():
                if type(key) is not str:raise NativeJsonCompatibilityError('JSON key type')
                k=key.casefold()
                if k in folded:raise NativeJsonCompatibilityError('ambiguous native JSON keys')
                folded.add(k);todo.append((child,depth+1))
        elif type(value) is list:
            categories={isinstance(child,(dict,list)) for child in value}
            if len(categories)>1:raise NativeJsonCompatibilityError('mixed native JSON array categories')
            todo.extend((child,depth+1) for child in value)
        elif type(value) is str and value.startswith('\\#'):
            raise NativeJsonCompatibilityError('reserved native JSON value prefix')
    return {'comparison_representation_checked':True,'nodes':nodes,'complete_gameplay_schema':False}
