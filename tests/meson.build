doctest_dep = dependency('doctest')

test_deps = [lunar_core_dep, lunar_types_dep, doctest_dep]
test_incs = include_directories('../src/lunar')

# --- type marshalling tests ---
test_types = executable('test_types',
    'test_types.cpp',
    dependencies: test_deps,
    include_directories: test_incs,
)
test('types', test_types)

# --- sandbox tests ---
test_sandbox = executable('test_sandbox',
    'test_sandbox.cpp',
    dependencies: test_deps,
    include_directories: test_incs,
)
test('sandbox', test_sandbox)

# --- JSON module tests ---
test_json = executable('test_json',
    'test_json.cpp',
    dependencies: test_deps,
    include_directories: test_incs,
)
test('json', test_json)
