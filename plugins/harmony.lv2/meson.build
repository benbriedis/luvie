
lv2dep = dependency('lv2')

shared_library(
    'harmony',
    'harmony.c',
    name_prefix: '',
    dependencies: [lv2dep]
)

fs = import('fs')

fs.copyfile('harmony.ttl', 'harmony.ttl')
fs.copyfile('manifest.ttl', 'manifest.ttl')

