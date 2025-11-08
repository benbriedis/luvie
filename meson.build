project('songEditor', 'c',
    version : '0.1',
    default_options : ['warning_level=3'])

lv2dep = dependency('lv2')
lilvdep = dependency('lilv-0')


shared_library(
    'songEditor.lv2/songEditor',
    'songEditor/songEditor.c',
    'songEditor/pluginLoader.c',
    name_prefix: '',
    dependencies: [lv2dep,lilvdep]
)

fs = import('fs')
fs.copyfile('songEditor/songEditor.ttl', 'songEditor.lv2')
fs.copyfile('songEditor/manifest.ttl', 'songEditor.lv2')


# Harmony:

shared_library(
    'plugins/harmony/harmony',
    'plugins/harmony/harmony.c',
    name_prefix: '',
    dependencies: [lv2dep]
)

fs.copyfile('plugins/harmony/harmony.ttl', 'plugins/harmony.lv2')
fs.copyfile('plugins/harmony/manifest.ttl', 'plugins/harmony.lv2')

