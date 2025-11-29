
#XXX probably better as a (shared) subproject?
#lv2dep = dependency('lv2')
lv2_dep = subproject('lv2').get_variable('lv2_dep')

shared_library(
    'harmony',
    'harmony.c',
    name_prefix: '',
    dependencies: [lv2_dep]
)

fs = import('fs')

fs.copyfile('harmony.ttl', 'harmony.ttl')
fs.copyfile('manifest.ttl', 'manifest.ttl')

