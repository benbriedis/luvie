
lv2dep = dependency('lv2')
lilvdep = dependency('lilv-0')

shared_library(
    'songEditor',
    'songEditor.c',
    'pluginLoader.c',
    name_prefix: '',
    dependencies: [lv2dep,lilvdep]
)

fs = import('fs')
fs.copyfile('songEditor.ttl', 'songEditor.ttl')
fs.copyfile('manifest.ttl', 'manifest.ttl')

