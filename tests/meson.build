# Get the testing library
# catch2_proj = subproject('catch2')
gtest_dep = dependency('gtest')


features_test = executable('features_test', 'features.cc',
  dependencies: [regex_backend_dep, gtest_dep]
  )

presets_test = executable('presets_test', 'presets.cc',
  dependencies: [regex_backend_dep, gtest_dep])

test('features', features_test)
test('presets', presets_test)
