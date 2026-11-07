#ifndef LID_MATERIAL_READ_SCOPE_R152_H
#define LID_MATERIAL_READ_SCOPE_R152_H
#ifndef LID_MATERIAL_PICKUP_OPTIMIZED_ENABLED
#define LID_MATERIAL_PICKUP_OPTIMIZED_ENABLED 0
#endif
/* Cache VM region metadata only during one read-only traversal. Every actual
   object/field/name read remains fresh. Never span engine dispatch or writes. */
#if LID_MATERIAL_PICKUP_OPTIMIZED_ENABLED
static int checks_chest_scope_begin(void*);
static void checks_chest_scope_end(void*,int);
#define R152_READ_BEGIN() checks_chest_scope_begin(0)
#define R152_READ_END(acquired) checks_chest_scope_end(0,acquired)
#else
#define R152_READ_BEGIN() 0
#define R152_READ_END(acquired) ((void)(acquired))
#endif
#endif
