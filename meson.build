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

# sources
src = files(
    'src/main.cpp',
    'src/runtime/isolate.cpp',
    'src/runtime/sandbox.cpp',
    'src/server/server.cpp',
    'src/server/connection.cpp',
    'src/pool/isolate_pool.cpp',
    'src/utils/logger.cpp',
    'src/utils/http.cpp',
    'src/utils/bootstrap.cpp',
    'src/stdlib/json/json.cpp',
    'src/stdlib/fetch/fetch.cpp',
    'src/stdlib/fetch/curl_uv_context.cpp'
)

# main executable
executable(
    'lunar',
    src,
    dependencies: [lua_lunar, libuv, openssl, llhttp, spdlog, yyjson, libcurl],
    include_directories: include_directories('src'),
    install: true,
)

# all source files for tooling
all_src = [
    'src/main.cpp',
    'src/runtime/isolate.cpp',
    'src/runtime/isolate.hpp',
    'src/runtime/sandbox.cpp',
    'src/runtime/sandbox.hpp',
    'src/server/server.cpp',
    'src/server/server.hpp',
    'src/server/connection.cpp',
    'src/server/connection.hpp',
    'src/pool/isolate_pool.cpp',
    'src/pool/isolate_pool.hpp',
    'src/utils/logger.hpp',
    'src/utils/http.cpp',
    'src/utils/http.hpp',
    'src/utils/logger.cpp',
    'src/stdlib/json/json.cpp',
    'src/stdlib/json/json.hpp',
    'src/stdlib/fetch/curl_uv_context.cpp',
    'src/stdlib/fetch/curl_uv_context.hpp',
    'src/stdlib/fetch/fetch_limits.cpp',
    'src/stdlib/fetch/fetch.cpp',
    'src/stdlib/fetch/fetch.hpp'
]

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