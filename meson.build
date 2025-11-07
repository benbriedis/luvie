project('songEditor', 'c',
    version : '0.1',
    default_options : ['warning_level=3'])

lv2dep = dependency('lv2')
lilvdep = dependency('lilv-0')

shared_library(
    'songEditor.lv2/songEditor',
    'songEditor/songEditor.c',
    'songEditor/callLuvie.c',
    name_prefix: '',
    dependencies: [lv2dep,lilvdep]
)

fs = import('fs')
fs.copyfile('songEditor/songEditor.ttl', 'songEditor.lv2')
fs.copyfile('songEditor/manifest.ttl', 'manifest.lv2')

