
# Pugl dependencies:
# Currently just installing globally. Note it has dependencies on cmake and python. 
# Does Pugl really need it? I think its a testing thing
# EASIEST approach appears to apply a 'diff' file in the wrap
#glslang_dep = subproject('glslang').get_variable('glslang_dep')
pugl_dep = subproject('pugl').get_variable('pugl_dep')

cairo_dep = dependency(
  'cairo',
  include_type: 'system',
)

incdir = include_directories('../subprojects/pugl.dir/subprojects/puglutil/include')

executable('testui', 'main.c', dependencies: [pugl_dep,cairo_dep],include_directories:indir)
