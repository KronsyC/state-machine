# Get the testing library
# catch2_proj = subproject('catch2')
gtest_dep = dependency('gtest')


features_test = executable('features_test', 'features.cc',
  dependencies: [regex_backend_dep, gtest_dep]
  )


test('Basic features', features_test)
