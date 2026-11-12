# Loaded only on a fresh, keyless packaging runner. Installation was staged by
# the build job, so do not execute its generated install scripts or find Qt.
include("${CMAKE_CURRENT_LIST_DIR}/../../build/windows/CPackConfig.cmake")
set(CPACK_INSTALL_CMAKE_PROJECTS "")
get_filename_component(release_build "${CMAKE_CURRENT_LIST_DIR}/../../build/windows" ABSOLUTE)
set(CPACK_INSTALLED_DIRECTORIES "${release_build}/payload" ".")
set(CPACK_PACKAGE_DIRECTORY "${release_build}")
