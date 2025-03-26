cmake_minimum_required(VERSION 3.16.0)

if(ARCH_FLAGS)
	if (WIN32)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch:${ARCH_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=${ARCH_FLAGS}")
	endif()
endif()

add_subdirectory(egtb/dictzip)

project(egtb)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENG_SRC 
	benchmark.cpp
	bitbase.cpp
	bitboard.cpp
	bitboard.h
	endgame.cpp
	endgame.h
	evaluate.cpp
	evaluate.h
	material.cpp
	material.h
	misc.cpp
	misc.h
	movegen.cpp
	movegen.h
	movepick.cpp
	movepick.h
	pawns.cpp
	pawns.h
	psqt.cpp
	position.cpp
	position.h
	search.cpp
	search.h
	thread.cpp
	thread.h
	thread_win32_osx.h
	timeman.cpp
	timeman.h
	tt.cpp
	tt.h
	tune.cpp
	tune.h
	types.h
	uci.cpp
	uci.h
	ucioption.cpp
	#
	syzygy/tbprobe.cpp
	syzygy/tbprobe.h
	#
	incbin/incbin.h
)
SOURCE_GROUP ("src" FILES ${ENG_SRC})

set(EGTB_SRC 
	egtb/elements.cpp
	egtb/elements.h
	egtb/tb_reader.cpp
	egtb/tb_reader.h
	egtb/tb_idx.cpp
	egtb/tb_idx.h
	#
	egtb/tb_api.cpp
	egtb/tb_api.h
)
SOURCE_GROUP ("EGTB" FILES ${EGTB_SRC})

add_library(${PROJECT_NAME} STATIC
	${ENG_SRC}
	${EGTB_SRC}
)

target_compile_definitions(${PROJECT_NAME}
	#PRIVATE UNICODE
	PRIVATE ANTI
	PRIVATE USE_POPCNT
	PRIVATE USE_PEXT
)

target_link_libraries(${PROJECT_NAME}
	$<TARGET_OBJECTS:dictzip>
)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
