project('web-power-switch', 'cpp',
  version : '0.2',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

cmake = import('cmake')

absl_strings_dep = dependency('absl_strings', fallback : ['abseil-cpp', 'absl_strings_dep'])
libcxxopts_dep = dependency('cxxopts', fallback : ['cxxopts', 'cxxopts_dep'])

libcrypto_dep = dependency('libcrypto')
libcurl_dep = dependency('libcurl')
libssl_dep = dependency('libssl')
tidy_dep = dependency('tidy')
yamlcpp_dep = dependency('yaml-cpp')

#cxxopts_proj = cmake.subproject('cxxopts')
#cxxopts_dep = cxxopts_proj.dependency('cxxopts')

wps_inc = include_directories('lib')
subdir('lib')


executable('pwrcntrl',
           'pwrcntrl.cc',
           include_directories : wps_inc,
           dependencies : [
                          absl_strings_dep,
                          libcxxopts_dep,
                          libcrypto_dep,
                          libcurl_dep,
                          libssl_dep,
                          tidy_dep,
                          yamlcpp_dep,
                          ],
           link_with : wpslib,
           install : true,
           )
