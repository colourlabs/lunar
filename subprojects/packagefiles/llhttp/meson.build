project('llhttp', 'c', version: '9.4.1')

llhttp_inc = include_directories('include')
llhttp_src = files('src/llhttp.c', 'src/api.c', 'src/http.c')
llhttp_lib = static_library('llhttp', llhttp_src,
    include_directories: llhttp_inc,
)
llhttp_dep = declare_dependency(
    include_directories: llhttp_inc,
    link_with: llhttp_lib,
)