project(
    'lunar',
    'cpp',
    version: '0.1.0',
    license: 'Apache-2.0',
    meson_version: '1.8.5',
    default_options: [
        'cpp_std=c++20',
        'warning_level=3',
        'buildtype=debugoptimized',
    ],
)

if get_option('buildtype') == 'debugoptimized' or get_option('profile')
    add_project_arguments(
        '-fno-omit-frame-pointer',
        language: ['c', 'cpp'],
    )
endif

if get_option('profile')
    add_project_arguments(
        '-fno-inline-small-functions',
        '-g',
        '-O2',
        language: ['c', 'cpp'],
    )
endif

# dependencies
lua_lunar = dependency('lua_lunar',
    fallback: ['lua', 'lua_dep'],
    required: true,
)
libuv = dependency('libuv',
    required: true,
)
openssl = dependency('openssl',
    required: true,
)
llhttp = dependency('llhttp',
    fallback: ['llhttp', 'llhttp_dep'],
    required: true,
)
spdlog = dependency('spdlog', 
    required: true
)
yyjson = dependency('yyjson',
    fallback: ['yyjson', 'yyjson_dep'],
    required: true,
)
libcurl = dependency('libcurl', 
    fallback: ['curl', 'curl_dep'], 
    required: true
)

# embedded Lua (convert .lua files to C++ header at build time)
embed_lua_script = files('scripts/embed_lua.py')

embedded_lua_hpp = custom_target('embedded-lua',
    input: files('src/lunar/lua/router.lua'),
    output: 'embedded_lua.hpp',
    command: [embed_lua_script, meson.current_source_dir() / 'src/lunar/lua', '@OUTPUT@'],
    build_by_default: true,
)

# core library sources
core_src = files(
    'src/core/runtime/isolate.cpp',
    'src/core/runtime/sandbox.cpp',
    'src/core/utils/logger.cpp',
    'src/core/utils/bootstrap.cpp',
    'src/core/stdlib/json/json.cpp',
    'src/core/stdlib/fetch/fetch.cpp',
    'src/core/stdlib/fetch/curl_uv_context.cpp',
)

# core library
lunar_core = static_library(
    'lunar_core',
    core_src + [embedded_lua_hpp],
    dependencies: [lua_lunar, libuv, openssl, llhttp, spdlog, yyjson, libcurl],
    include_directories: include_directories('src/core'),
    install: true,
)

lunar_core_dep = declare_dependency(
    link_with: lunar_core,
    include_directories: include_directories('src/core'),
    dependencies: [lua_lunar, libuv, openssl, llhttp, spdlog, yyjson, libcurl],
)

# lunar types library (HTTP wire types, shared between lunar and tests)
lunar_types_lib = static_library(
    'lunar_types',
    files('src/lunar/types.cpp'),
    include_directories: include_directories('src/lunar'),
)

lunar_types_dep = declare_dependency(
    link_with: lunar_types_lib,
    include_directories: include_directories('src/lunar'),
)

# lunar executable sources
lunar_src = files(
    'src/lunar/main.cpp',
    'src/lunar/server/server.cpp',
    'src/lunar/server/connection.cpp',
    'src/lunar/pool/isolate_pool.cpp',
    'src/lunar/utils/http.cpp',
)

# main executable
executable(
    'lunar',
    lunar_src,
    dependencies: [lunar_core_dep, lunar_types_dep],
    include_directories: include_directories('src/lunar'),
    install: true,
)

# Lua stdlib files
install_subdir('src/lunar/lua',
    install_dir: 'share/lunar',
    strip_directory: true,
)

# all source files for tooling
all_src = [
    'src/core/runtime/isolate.cpp',
    'src/core/runtime/isolate.hpp',
    'src/core/runtime/sandbox.cpp',
    'src/core/runtime/sandbox.hpp',
    'src/core/utils/logger.hpp',
    'src/core/utils/logger.cpp',
    'src/core/utils/bootstrap.cpp',
    'src/core/utils/bootstrap.hpp',
    'src/core/stdlib/json/json.cpp',
    'src/core/stdlib/json/json.hpp',
    'src/core/stdlib/fetch/curl_uv_context.cpp',
    'src/core/stdlib/fetch/curl_uv_context.hpp',
    'src/core/stdlib/fetch/fetch_limits.hpp',
    'src/core/stdlib/fetch/fetch.cpp',
    'src/core/stdlib/fetch/fetch.hpp',
    'src/lunar/main.cpp',
    'src/lunar/types.cpp',
    'src/lunar/types.hpp',
    'src/lunar/server/server.cpp',
    'src/lunar/server/server.hpp',
    'src/lunar/server/connection.cpp',
    'src/lunar/server/connection.hpp',
    'src/lunar/pool/isolate_pool.cpp',
    'src/lunar/pool/isolate_pool.hpp',
    'src/lunar/utils/http.cpp',
    'src/lunar/utils/http.hpp',
]

# tests
if not meson.is_subproject()
    subdir('tests')
endif

run_target('lint',
    command: [
        'clang-tidy',
        '-p', meson.project_build_root(),
        '--checks=-modernize-*',
    ] + all_src + [
        'subprojects/lua/lunar/lunar_alloc.c',
        'subprojects/lua/lunar/lunar_limits.c',
    ]
)

run_target('format',
    command: [
        'clang-format',
        '-i',
    ] + all_src
)