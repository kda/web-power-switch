wps_sources = [
  'md5Helper.cc',
  'tidyHelper.cc',
  'tidydocwrapper.cc',
  'trim.cc',
  'webpowerswitch.cc',
  'webpowerswitchmanager.cc',
  ]

# create empty default if installed version is new enough
abseil_inc = include_directories()

absl_strings_dep = dependency('absl_strings', version : '>=20220623', required : false)
if not absl_strings_dep.found()
  abseil_proj = subproject('abseil-cpp', required : false)
  absl_strings_dep = abseil_proj.get_variable('absl_strings_dep')
  abseil_inc = abseil_proj.get_variable('absl_include_dir')
endif

libcrypto_dep = dependency('libcrypto')
libcurl_dep = dependency('libcurl')
tidy_dep = dependency('tidy', static: true)
yamlcpp_dep = dependency('yaml-cpp', static: true)

wps_dependencies = [
  absl_strings_dep,
  libcrypto_dep,
  libcurl_dep,
  tidy_dep,
  yamlcpp_dep,
  ]

wps_lib = library(
    'wps',
    wps_sources,
    include_directories : [abseil_inc],
    dependencies : wps_dependencies,
    )

wps_dep = declare_dependency(
    link_with : wps_lib,
    include_directories : include_directories('.'),
    dependencies : [absl_strings_dep],
  )

