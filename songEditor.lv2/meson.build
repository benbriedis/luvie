
#lv2dep = dependency('lv2')

lv2_dep = subproject('lv2').get_variable('lv2_dep')

#XXX probably not use this, at least initially
# Lilv dependencies:
#lilvdep = dependency('lilv-0')
sord_dep = subproject('sord').get_variable('sord_dep')
lilv_dep = subproject('lilv').get_variable('lilv_dep')

shared_library(
    'songEditor',
    'songEditor.c',
    'pluginLoader.c',
    name_prefix: '',
    dependencies: [lv2_dep,sord_dep,lilv_dep]
)

fs = import('fs')
fs.copyfile('songEditor.ttl', 'songEditor.ttl')
fs.copyfile('manifest.ttl', 'manifest.ttl')

