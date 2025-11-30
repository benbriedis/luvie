
cmake = import('cmake')

#cmake_opts = cmake.subproject_options()
#cmake_opts.add_cmake_defines({'CMAKE_POSITION_INDEPENDENT_CODE': true})
#cmake_opts.set_override_option('cpp_std', 'c++11')  #XXX perhaps make global?

#CFLAGS=-Iincludedir; export CFLAGS
#CXXFLAGS=-Iincludedir; export CXXFLAGS
#LDFLAGS=-Llibdir; export LDFLAGS

#fltk_proj = cmake.subproject('fltk', options: cmake_opts)
#fltk_proj = cmake.subproject('fltk')

#fltk_lib = fltk_proj.dependency('fltk')
#my_fltk_dep = declare_dependency(link_with :'fltk.dir/lib/libfltk.a', include_directories : 'fltk.dir/include')

#TODO proably remove the .dir suffixes
#my_fltk_dep = declare_dependency(d_import_dirs: '/home/ben/programming/luvie/build/subprojects/fltk.dir')
#my_fltk_dep = declare_dependency(d_import_dirs: '/home/ben/programming/temp/fltk/build')
my_fltk_dep = declare_dependency(
  d_import_dirs: '/home/ben/programming/luvie/subprojects/fltk.dir/install',
  #  include_directories: '/home/ben/programming/luvie/subprojects/fltk.dir/install/include',
  include_directories: 'subprojects/fltk.dir/install/include',
)



executable('testui2', 'main.c', dependencies: [my_fltk_dep])
