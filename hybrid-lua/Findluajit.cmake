include(ExternalProject)

set(LUAJIT_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/luajit)
set(LUAJIT_INSTALL_DIR ${LUAJIT_PREFIX}/install)
set(LUAJIT_INSTALL_INCLUDE_DIR ${LUAJIT_INSTALL_DIR}/include/luajit-2.1/)
# This MAKE_DIRECTORY is important, because this directory needs to exist when we set
# the include directories of the target below. Otherweise it would only be created
# by the install step and configure would fail.
file(MAKE_DIRECTORY ${LUAJIT_INSTALL_INCLUDE_DIR})

if(UNIX)
    set(LUAJIT_LIBRARY_NAME libluajit-5.1.a)

    ExternalProject_Add(
        luajit_ep
        URL https://github.com/LuaJIT/LuaJIT/archive/refs/tags/v2.1.0-beta3.tar.gz
        #URL_HASH MD5=eae40bc29d06ee5e3078f9444fcea39b
        PREFIX ${LUAJIT_PREFIX}
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make
        INSTALL_DIR ${LUAJIT_INSTALL_DIR}
        INSTALL_COMMAND make install PREFIX=<INSTALL_DIR>
        BUILD_IN_SOURCE 1
        BUILD_BYPRODUCTS <INSTALL_DIR>/lib/${LUAJIT_LIBRARY_NAME}
    )
elseif(MSVC)
    # After painstakingly building the UNIX version of this, I asked ChatGPT to 
    # create the Windows version, because I can't test it myself, so this is
    # untested, but hopefully a good starting point in the future
    set(LUAJIT_LIBRARY_NAME lua51.lib)
    
    ExternalProject_Add(
        luajit_ep
        URL http://luajit.org/download/LuaJIT-2.1.0-beta3.zip
        URL_HASH MD5=eae40bc29d06ee5e3078f9444fcea39b
        PREFIX ${LUAJIT_PREFIX}
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND nmake /f msvcbuild.mak
        INSTALL_COMMAND nmake /f msvcbuild.mak install PREFIX=<INSTALL_DIR>
        INSTALL_DIR ${LUAJIT_INSTALL_DIR}
        BUILD_IN_SOURCE 1
        BUILD_BYPRODUCTS <INSTALL_DIR>/lib/${LUAJIT_LIBRARY_NAME}
    )
endif()

# We need to use this IMPORTED library with IMPORTED_LOCATION, so CMake will not complain
# during configure, that the library does not exist.
add_library(luajit STATIC IMPORTED)
add_dependencies(luajit luajit_ep)
set_target_properties(luajit PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${LUAJIT_INSTALL_INCLUDE_DIR}
    IMPORTED_LOCATION ${LUAJIT_INSTALL_DIR}/lib/${LUAJIT_LIBRARY_NAME}
)

# Set the variables that `find_package` would normally set
set(luajit_FOUND TRUE)
#set(luajit_INCLUDE_DIRS ${LUAJIT_INSTALL_INCLUDE_DIR})
#set(luajit_LIBRARIES ${LUAJIT_INSTALL_DIR}/lib/${LUAJIT_LIBRARY_NAME})
