#ifndef GUARD_GLSL_GUARD
#define GUARD_GLSL_GUARD

#define guard(expr) if (!(expr)) { return; }
#define guard_continue(expr) if (!(expr)) { continue; }
#define guard_break(expr) if (!(expr)) { break; }

#endif // GUARD_GLSL_GUARD