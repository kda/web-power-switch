project('web-power-switch', 'cpp',
  version : '0.2',
  meson_version : '>=0.65.0',
  default_options : [
    'warning_level=3',
    'cpp_std=c++14',
    ])

# create empty default if installed version is new enough
abseil_inc = include_directories()

# TODO: migrate to lib/meson.build (since that is where it is first used and needs to be present for users of lib
absl_strings_dep = dependency('absl_strings', version : '>=20220623', required : false)
if not absl_strings_dep.found()
  abseil_proj = subproject('abseil-cpp', required : false)
  absl_strings_dep = abseil_proj.get_variable('absl_strings_dep')
  abseil_inc = abseil_proj.get_variable('absl_include_dir')
endif

cxxopts_dep = dependency('cxxopts', version : '>=3.0.0', fallback : ['cxxopts', 'cxxopts_dep'])

libcrypto_dep = dependency('libcrypto')
libcurl_dep = dependency('libcurl')
libssl_dep = dependency('libssl')
tidy_dep = dependency('tidy')
yamlcpp_dep = dependency('yaml-cpp')

subdir('lib')

executable('pwrcntrl',
           'pwrcntrl.cc',
           include_directories : [wps_inc, abseil_inc],
           dependencies : [
                          absl_strings_dep,
                          cxxopts_dep,
                          libcrypto_dep,
                          libcurl_dep,
                          libssl_dep,
                          tidy_dep,
                          yamlcpp_dep,
                          ],
           link_with : wps_lib,
           )
