project(
    'lunar',
    'cpp',
    version: '0.1.0',
    license: 'Apache-2.0',
    default_options: [
        'cpp_std=c++20',
        'warning_level=3',
        'buildtype=debugoptimized',
    ],
)

# dependencies
lua = dependency('lua5.5',
    fallback: ['lua', 'lua_dep'],
    required: true,
)

libuv = dependency('libuv',
    required: true,
)

openssl = dependency('openssl',
    required: true,
)

# sources
src = files(
    'src/main.cpp',
    'src/runtime/isolate.cpp',
    'src/runtime/sandbox.cpp',
)

# main executable
executable(
    'lunar',
    src,
    dependencies: [lua, libuv, openssl],
    include_directories: include_directories('src'),
    install: true,
)

run_target('lint',
    command: [
        'clang-tidy',
        '-p', meson.project_build_root(),
        '--checks=-modernize-*',  # disable modernize for C files
        'subprojects/lua/lunar/lunar_alloc.c',
        'subprojects/lua/lunar/lunar_limits.c',
    ]
)

run_target('format',
    command: [
        'clang-format',
        '-i',
        'src/runtime/isolate.cpp',
        'src/runtime/isolate.h',
        'src/runtime/sandbox.cpp',
        'src/runtime/sandbox.h',
        'src/main.cpp',
    ]
)