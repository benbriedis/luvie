# Per-generator packaging settings.
#
# CPack includes this file once for each generator it runs, with CPACK_GENERATOR set to
# that single generator -- unlike cmake/Packaging.cmake, which is evaluated at configure
# time when the generator list is still just a list. It is wired up there through
# CPACK_PROJECT_CONFIG_FILE.
#
# The plug-in's licence texts install to two different places, one component each (see
# src/lv2/CMakeLists.txt). Exactly one belongs in any given package, so drop the other:
#
#   DEB       installing the package is the whole procedure, and what it leaves behind is
#             luvie.lv2 on the LV2 path -- so the licences go in there. A top-level
#             /LICENSES would land in the filesystem root, which is nobody's install.
#   archives  unpacking leaves a directory, not an installation, so LICENSES/ goes at the
#             top of it where it is visible rather than several levels down under lib/.
if(CPACK_GENERATOR STREQUAL "DEB")
    list(REMOVE_ITEM CPACK_COMPONENTS_ALL PluginArchiveLicense)
else()
    list(REMOVE_ITEM CPACK_COMPONENTS_ALL PluginLicense)
endif()
