# Included after every project() call (CMAKE_PROJECT_INCLUDE in toolchain.cmake). CMake's Generic platform
# declares that shared libraries are unsupported, which makes subprojects that merely declare one fail to
# configure (ztd.idk's ztd.blocks). Nothing shared is ever built for the PS5 title.
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
