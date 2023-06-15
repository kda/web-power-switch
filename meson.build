project('web-power-switch', 'cpp',
  version : '0.2',
  meson_version : '>=0.65.0',
  default_options : [
    'warning_level=3',
    'cpp_std=c++14',
    ])

cxxopts_dep = dependency('cxxopts', version : '>=3.0.0', fallback : ['cxxopts', 'cxxopts_dep'])

subdir('lib')

executable('pwrcntrl',
           'pwrcntrl.cc',
           dependencies : [
                          cxxopts_dep,
                          wps_dep,
                          ],
           )
