project('web-power-switch', 'cpp',
  version : '0.2',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

cmake = import('cmake')

libcxxopts_dep = dependency('cxxopts', version : '>=3.0.0', fallback : ['cxxopts', 'cxxopts_dep'])

libcrypto_dep = dependency('libcrypto')
libcurl_dep = dependency('libcurl')
libssl_dep = dependency('libssl')
tidy_dep = dependency('tidy')
yamlcpp_dep = dependency('yaml-cpp')

wps_inc = include_directories('lib')
subdir('lib')


executable('pwrcntrl',
           'pwrcntrl.cc',
           include_directories : wps_inc,
           dependencies : [
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
