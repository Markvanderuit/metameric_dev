#version 460 core

#define guard(expr)          if (!(expr)) { return; }
#define guard_continue(expr) if (!(expr)) { continue; }
#define guard_break(expr)    if (!(expr)) { break; }