cmake_minimum_required (VERSION 3.13.0)

project(losingloeser LANGUAGES C)

if(ARCH_FLAGS)
	if (WIN32)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch:${ARCH_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=${ARCH_FLAGS}")
	endif()
endif()

file(GLOB SRC_LL
	*.c
	*.h
)
set(LL_SRC ${SRC_LL})
SOURCE_GROUP ("src" FILES ${LL_SRC})

add_library(${PROJECT_NAME}
	${LL_SRC}
)

target_include_directories(${PROJECT_NAME}
	PRIVATE ../tb/egtb
)

target_compile_definitions(${PROJECT_NAME}
	PUBLIC HAS_POPCNT
)

target_link_libraries(${PROJECT_NAME}
	egtb
)
