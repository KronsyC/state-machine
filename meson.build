project('regex-backend', 'cpp')

add_global_arguments('-D_GLIBCXX_DEBUG -Wall -Werror -O0 -g', language : 'cpp')

inc = include_directories('include')

mutils_proj = subproject('mutils')
mutils_dep = mutils_proj.get_variable('mutils_dep')

regex_backend = static_library(
  'regex-backend',
  dependencies: mutils_dep,
  include_directories : inc,
  install : true,
  cpp_args: ['-std=c++20']
)

regex_backend_dep = declare_dependency(
  include_directories : inc,
  link_with : regex_backend,
  dependencies: mutils_dep
  )

subdir('tests')
