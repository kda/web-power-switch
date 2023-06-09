wps_sources = [
  'md5Helper.cc',
  'tidyHelper.cc',
  'tidydocwrapper.cc',
  'trim.cc',
  'webpowerswitch.cc',
  'webpowerswitchmanager.cc',
  ]

wps_inc = include_directories('.')

wps_lib = static_library(
    'wps',
    wps_sources,
    include_directories : [abseil_inc],
    )

