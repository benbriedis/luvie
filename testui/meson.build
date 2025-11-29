
# Pugl dependencies:
glslang_dep = subproject('glslang').get_variable('glslang_dep')
pugl_dep = subproject('pugl').get_variable('pugl_dep')

executable('testui', 'main.c', dependencies: [glslang_dep,pugl_dep])
