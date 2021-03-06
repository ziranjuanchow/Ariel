cmake_minimum_required(VERSION 3.0.0)

project(ariel)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/build/cmake")

#Nuparu location (alternatively, /usr/local or /opt/local if not using Nuparu)
set(NUPARU "../Nuparu")

#Set up include and lib paths
include_directories(${NUPARU}/include)
include_directories(${NUPARU}/src)
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${NUPARU}/lib/osx)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${NUPARU}/lib/linux /usr/lib64)
elseif(WIN32)
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${NUPARU}/lib/win)
endif()

set(GLFW_INCLUDE_DIR ${NUPARU}/include)
set(GLFW_LIBRARY_DIR ${CMAKE_LIBRARY_PATH})
set(GLEW_INCLUDE_DIR ${NUPARU}/include)
set(GLEW_LIBRARY_DIR ${CMAKE_LIBRARY_PATH})

#find up and set up core dependency libs
find_library(GLFW_LIBRARY "glfw3" HINTS ${GLFW_LIBRARY_DIR})
find_package(OPENGL)
find_package(GLEW)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    find_package(GLUT)
endif()

find_library(OPENVDB "openvdb")
find_library(PARTIO "partio")

#OpenVDB dynamic links these, so we need to as well
find_library(TBB "tbb")
find_library(HALF "half")
find_library(LIBZ "z")
find_library(JSONCPP "jsoncpp")

set(VDBLIBS ${HALF} ${LIBZ} ${TBB})
set(CORELIBS ${GLFW_LIBRARY} ${GLUT_LIBRARY} ${OPENGL_LIBRARY} ${GLEW_LIBRARY} ${OPENVDB}
   ${VDBLIBS} ${PARTIO} ${JSONCPP})

# OSX-specific hacks/fixes
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    #Link IOKit because this is where we get GL stuff for OSX
    set(COREVIDEO "-framework CoreVideo")
    set(IOKIT "-framework IOKit")
    set(CORELIBS ${CORELIBS} ${IOKIT} ${COREVIDEO})
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# Linux specific hacks/fixes
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lX11 -lXxf86vm -lXrandr -lpthread -lXi")
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3 -m64 -msse2 -w")
elseif(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
endif()

if(MSVC)
    set(CompilerFlags
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE
            )
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
    # /bigobj is required for windows builds because of the size of
    # some object files (CastImage for instance)
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj" )
    set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /bigobj" )
endif()

set(SOURCE_FILES "src/main.cpp"
                 "src/sim/flip.cpp"
                 "src/grid/particlegrid.cpp"
                 "src/geom/geom.cpp"
                 "src/geom/mesh.cpp"
                 "src/geom/spheregen.cpp"
                 "src/geom/cubegen.cpp"
                 "src/camera/camera.cpp"
                 "src/camera/perspcam/perspcam.cpp"
                 "src/geom/obj/obj.cpp"
                 "src/scene/scene.cpp"
                 "src/scene/sceneloader.cpp"
                 "src/viewer/viewer.cpp"
                 "src/grid/levelset.cpp"
                 "src/ray/ray.cpp"
                 "src/spatial/aabb.cpp"
                 "src/spatial/spatial.cpp"
                 "${NUPARU}/src/stb_image/stb_image.c"
                 "${NUPARU}/src/stb_image/stb_image_write.c"
                 "${NUPARU}/src/rmsd/rmsd.c"
                 )

add_executable(ariel ${SOURCE_FILES})

target_link_libraries(ariel ${CORELIBS})
