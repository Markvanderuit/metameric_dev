#ifndef PREAMBLE_GLSL_GUARD
#define PREAMBLE_GLSL_GUARD

#define wrap_symbol(x) x
#define hash_symbol #
#define extension(name, set) wrap_symbol(hash_symbol)extension name : set
#define version(num, type)   wrap_symbol(hash_symbol)version num type
#define include(file_path)   wrap_symbol(hash_symbol)include file_path

version(460, core)

#endif // PREAMBLE_GLSL_GUARD
