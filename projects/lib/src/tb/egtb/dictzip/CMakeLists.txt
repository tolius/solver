cmake_minimum_required (VERSION 3.13.0)

project(dictzip LANGUAGES C)

if (ARCH_FLAGS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arch:${ARCH_FLAGS}")
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

target_compile_definitions(${PROJECT_NAME}
	PRIVATE _CRT_SECURE_NO_WARNINGS
)

target_compile_options(${PROJECT_NAME}
	PRIVATE "/Ox"
	PRIVATE "/GL"
)
