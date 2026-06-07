project('lua', 'c', version: '5.5.0')

lua_inc = include_directories('src', 'lunar')

lua_src = files(
    'src/lapi.c',
    'src/lcode.c',
    'src/lctype.c',
    'src/ldebug.c',
    'src/ldo.c',
    'src/ldump.c',
    'src/lfunc.c',
    'src/lgc.c',
    'src/linit.c',
    'src/llex.c',
    'src/lmem.c',
    'src/lobject.c',
    'src/lopcodes.c',
    'src/lparser.c',
    'src/lstate.c',
    'src/lstring.c',
    'src/ltable.c',
    'src/ltm.c',
    'src/lundump.c',
    'src/lvm.c',
    'src/lzio.c',
    'src/lauxlib.c',
    'src/lbaselib.c',
    'src/lcorolib.c',
    'src/ldblib.c',
    'src/liolib.c',
    'src/lmathlib.c',
    'src/loslib.c',
    'src/lstrlib.c',
    'src/ltablib.c',
    'src/lutf8lib.c',
    'src/loadlib.c',
    'lunar/lunar_alloc.c',
    'lunar/lunar_limits.c',
)

lua_lib = static_library('lua', lua_src,
    include_directories: lua_inc,
    c_args: ['-DLUA_USE_LINUX'],
)

lua_dep = declare_dependency(
    include_directories: lua_inc,
    link_with: lua_lib,
)