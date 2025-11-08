project('luvie', 'c',
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
fs.copyfile('songEditor/songEditor.ttl', 'songEditor.ttl', install_dir:'build/songEditor.lv2')
fs.copyfile('songEditor/manifest.ttl', 'manifest.ttl', install_dir:'build/songEditor.lv2')


# Harmony:

shared_library(
    'plugins/harmony.lv2/harmony',
    'plugins/harmony/harmony.c',
    name_prefix: '',
    dependencies: [lv2dep]
)

fs.copyfile('plugins/harmony/harmony.ttl', 'harmony.ttl', install_dir:'build/plugins/harmony.lv2')
#fs.copyfile('plugins/harmony/manifest.ttl', 'manifest.ttl', install_dir:'build/plugins/harmony.lv2')

