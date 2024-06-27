cmake_minimum_required (VERSION 3.13.0)

project(dictzip LANGUAGES C)

if(ARCH_FLAGS)
	if (WIN32)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch:${ARCH_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
	endif()
endif()

file(GLOB SRC_APP
	*.c
	*.h
)
set(APP_SRC ${SRC_APP})
SOURCE_GROUP ("src" FILES ${APP_SRC})

file(GLOB SRC_ZLIB
	zlib/*.c
	zlib/*.h
)
set(ZLIB_SRC ${SRC_ZLIB})
SOURCE_GROUP ("zlib" FILES ${ZLIB_SRC})

include_directories(
	zlib
)

add_library(${PROJECT_NAME} OBJECT
	${APP_SRC}
	${ZLIB_SRC}
)
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE 1)

if (WIN32)
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE _CRT_SECURE_NO_WARNINGS
		PRIVATE HAVE_MMAP
	)
	target_compile_options(${PROJECT_NAME}
		PRIVATE "/Ox"
		PRIVATE "/GL"
	)
else()
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE HAVE_UNISTD_H
		PRIVATE HAVE_MMAP
		PRIVATE NDEBUG
	)
	target_compile_options(${PROJECT_NAME}
		PRIVATE "-Ofast"
		PRIVATE "-flto"
		PRIVATE "-march=native"
	)
endif()
