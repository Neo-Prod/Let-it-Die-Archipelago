#ifndef LID_CHARACTER_READ_SCOPE_R157_H
#define LID_CHARACTER_READ_SCOPE_R157_H
#ifndef LID_CHARACTER_READ_OPTIMIZED_ENABLED
#define LID_CHARACTER_READ_OPTIMIZED_ENABLED 0
#endif
/* Only the synchronous readonly helpers may use this scope. The cache holds
   VM-region metadata, not game values. Callers keep their original cadence. */
#if LID_CHARACTER_READ_OPTIMIZED_ENABLED && LID_REFLECTION_READ_SCOPE_ENABLED
#define R157_READ_SCOPED 1
#define R157_READ_NAME(name) name##_read_core
#else
#define R157_READ_SCOPED 0
#define R157_READ_NAME(name) name
#endif
#endif
