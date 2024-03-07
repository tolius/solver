cmake_minimum_required(VERSION 3.16.0)

add_subdirectory(projects/lib/src/tb/egtb/dictzip)

project(solver)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

set(SOLVER_VERSION "unknown" FORCE)
if(EXISTS ${CMAKE_SOURCE_DIR}/.version-solver)
	file(READ ${CMAKE_SOURCE_DIR}/.version-solver SOLVER_VERSION)
	string(REGEX REPLACE "(\r?\n)+" "" SOLVER_VERSION "${SOLVER_VERSION}")
endif()
set(CUTECHESS_VERSION "unknown")
if(EXISTS ${CMAKE_SOURCE_DIR}/.version)
	file(READ ${CMAKE_SOURCE_DIR}/.version CUTECHESS_VERSION)
	string(REGEX REPLACE "(\r?\n)+" "" CUTECHESS_VERSION "${CUTECHESS_VERSION}")
endif()

option(WITH_TESTS "Enable building of unit tests" OFF)

if (UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(QT_COMPONENTS Core Gui Widgets Concurrent Svg PrintSupport)
if(WITH_TESTS)
	enable_testing()
	set(QT_COMPONENTS ${QT_COMPONENTS} Test)
endif()

find_package(Qt6 COMPONENTS ${QT_COMPONENTS} Core5Compat QUIET)
if(NOT Qt6_FOUND)
	find_package(Qt5 5.15 REQUIRED COMPONENTS ${QT_COMPONENTS})
endif()

include(GNUInstallDirs)

message(STATUS "Setting up build for Solver version ${SOLVER_VERSION}")


set(ENG_SRC 
	projects/lib/src/tb/benchmark.cpp
	projects/lib/src/tb/bitbase.cpp
	projects/lib/src/tb/bitboard.cpp
	projects/lib/src/tb/bitboard.h
	projects/lib/src/tb/endgame.cpp
	projects/lib/src/tb/endgame.h
	projects/lib/src/tb/evaluate.cpp
	projects/lib/src/tb/evaluate.h
	projects/lib/src/tb/material.cpp
	projects/lib/src/tb/material.h
	projects/lib/src/tb/misc.cpp
	projects/lib/src/tb/misc.h
	projects/lib/src/tb/movegen.cpp
	projects/lib/src/tb/movegen.h
	projects/lib/src/tb/movepick.cpp
	projects/lib/src/tb/movepick.h
	projects/lib/src/tb/pawns.cpp
	projects/lib/src/tb/pawns.h
	projects/lib/src/tb/psqt.cpp
	projects/lib/src/tb/position.cpp
	projects/lib/src/tb/position.h
	projects/lib/src/tb/search.cpp
	projects/lib/src/tb/search.h
	projects/lib/src/tb/thread.cpp
	projects/lib/src/tb/thread.h
	projects/lib/src/tb/thread_win32_osx.h
	projects/lib/src/tb/timeman.cpp
	projects/lib/src/tb/timeman.h
	projects/lib/src/tb/tt.cpp
	projects/lib/src/tb/tt.h
	projects/lib/src/tb/tune.cpp
	projects/lib/src/tb/tune.h
	projects/lib/src/tb/types.h
	projects/lib/src/tb/uci.cpp
	projects/lib/src/tb/uci.h
	projects/lib/src/tb/ucioption.cpp
	#
	projects/lib/src/tb/syzygy/tbprobe.cpp
	projects/lib/src/tb/syzygy/tbprobe.h
	#
	projects/lib/src/tb/incbin/incbin.h
)
SOURCE_GROUP ("src" FILES ${ENG_SRC})

set(EGTB_SRC 
	projects/lib/src/tb/egtb/elements.cpp
	projects/lib/src/tb/egtb/elements.h
	projects/lib/src/tb/egtb/tb_reader.cpp
	projects/lib/src/tb/egtb/tb_reader.h
	projects/lib/src/tb/egtb/tb_idx.cpp
	projects/lib/src/tb/egtb/tb_idx.h
)
SOURCE_GROUP ("EGTB" FILES ${EGTB_SRC})

set(WATKINS_SRC
	projects/lib/src/watkins/watkinssolution.h
	projects/lib/src/watkins/watkinssolution.cpp
)
SOURCE_GROUP ("Watkins" FILES ${WATKINS_SRC})

add_library(lib STATIC
	${ENG_SRC}
	${EGTB_SRC}
	${WATKINS_SRC}
	projects/lib/src/board/boardfactory.cpp
	projects/lib/src/board/twokingseachboard.cpp
	projects/lib/src/board/placementboard.cpp
	projects/lib/src/board/oukboard.cpp
	projects/lib/src/board/makrukboard.cpp
	projects/lib/src/board/courierboard.cpp
	projects/lib/src/board/atomicboard.cpp
	projects/lib/src/board/knightrelayboard.cpp
	projects/lib/src/board/jesonmorboard.cpp
	projects/lib/src/board/antiboard.cpp
	projects/lib/src/board/dobutsushogiboard.cpp
	projects/lib/src/board/chessgiboard.cpp
	projects/lib/src/board/knightmateboard.cpp
	projects/lib/src/board/coregalboard.cpp
	projects/lib/src/board/rifleboard.cpp
	projects/lib/src/board/suicideboard.cpp
	projects/lib/src/board/hordeboard.cpp
	projects/lib/src/board/kingofthehillboard.cpp
	projects/lib/src/board/codrusboard.cpp
	projects/lib/src/board/seirawanboard.cpp
	projects/lib/src/board/loopboard.cpp
	projects/lib/src/board/racingkingsboard.cpp
	projects/lib/src/board/losalamosboard.cpp
	projects/lib/src/board/extinctionboard.cpp
	projects/lib/src/board/capablancaboard.cpp
	projects/lib/src/board/side.cpp
	projects/lib/src/board/aiwokboard.cpp
	projects/lib/src/board/modernboard.cpp
	projects/lib/src/board/frcboard.cpp
	projects/lib/src/board/embassyboard.cpp
	projects/lib/src/board/grandboard.cpp
	projects/lib/src/board/westernzobrist.cpp
	projects/lib/src/board/crazyhouseboard.cpp
	projects/lib/src/board/euroshogiboard.cpp
	projects/lib/src/board/standardboard.cpp
	projects/lib/src/board/minishogiboard.cpp
	projects/lib/src/board/berolinaboard.cpp
	projects/lib/src/board/connectboard.cpp
	projects/lib/src/board/andernachboard.cpp
	projects/lib/src/board/square.cpp
	projects/lib/src/board/gothicboard.cpp
	projects/lib/src/board/caparandomboard.cpp
	projects/lib/src/board/gustavboard.cpp
	projects/lib/src/board/janusboard.cpp
	projects/lib/src/board/aseanboard.cpp
	projects/lib/src/board/shogiboard.cpp
	projects/lib/src/board/chancellorboard.cpp
	projects/lib/src/board/pocketknightboard.cpp
	projects/lib/src/board/shatranjboard.cpp
	projects/lib/src/board/threekingsboard.cpp
	projects/lib/src/board/checklessboard.cpp
	projects/lib/src/board/restrictedmoveboard.cpp
	projects/lib/src/board/result.cpp
	projects/lib/src/board/boardtransition.cpp
	projects/lib/src/board/westernboard.cpp
	projects/lib/src/board/sittuyinboard.cpp
	projects/lib/src/board/gridboard.cpp
	projects/lib/src/board/judkinsshogiboard.cpp
	projects/lib/src/board/giveawayboard.cpp
	projects/lib/src/board/losersboard.cpp
	projects/lib/src/board/ncheckboard.cpp
	projects/lib/src/board/gryphonboard.cpp
	projects/lib/src/board/syzygytablebase.cpp
	projects/lib/src/board/zobrist.cpp
	projects/lib/src/board/board.cpp
	projects/lib/src/board/genericmove.cpp
	projects/lib/src/board/hoppelpoppelboard.cpp
	projects/lib/src/board/almostboard.cpp
	projects/lib/src/board/amazonboard.cpp
	projects/lib/src/board/chigorinboard.cpp

	projects/lib/src/enginetextoption.cpp
	projects/lib/src/openingsuite.cpp
	projects/lib/src/econode.cpp
	projects/lib/src/gamemanager.cpp
	projects/lib/src/roundrobintournament.cpp
	projects/lib/src/engineoption.cpp
	projects/lib/src/elo.cpp
	projects/lib/src/humanplayer.cpp
	projects/lib/src/tournamentpair.cpp
	projects/lib/src/chessplayer.cpp
	projects/lib/src/enginemanager.cpp
	projects/lib/src/knockouttournament.cpp
	projects/lib/src/moveevaluation.cpp
	projects/lib/src/playerbuilder.cpp
	projects/lib/src/epdrecord.cpp
	projects/lib/src/enginecombooption.cpp
	projects/lib/src/enginebuttonoption.cpp
	projects/lib/src/worker.cpp
	projects/lib/src/pgnstream.cpp
	projects/lib/src/pyramidtournament.cpp
	projects/lib/src/mersenne.cpp
	projects/lib/src/enginespinoption.cpp
	projects/lib/src/pgngame.cpp
	projects/lib/src/engineconfiguration.cpp
	projects/lib/src/tournament.cpp
	projects/lib/src/pgngameentry.cpp
	projects/lib/src/xboardengine.cpp
	projects/lib/src/timecontrol.cpp
	projects/lib/src/engineoptionfactory.cpp
	projects/lib/src/tournamentfactory.cpp
	projects/lib/src/humanbuilder.cpp
	projects/lib/src/chessgame.cpp
	projects/lib/src/openingbook.cpp
	projects/lib/src/enginefactory.cpp
	projects/lib/src/gauntlettournament.cpp
	projects/lib/src/gameadjudicator.cpp
	projects/lib/src/pgngamefilter.cpp
	projects/lib/src/uciengine.cpp
	projects/lib/src/enginecheckoption.cpp
	projects/lib/src/sprt.cpp
	projects/lib/src/chessengine.cpp
	projects/lib/src/polyglotbook.cpp
	projects/lib/src/enginebuilder.cpp
	projects/lib/src/tournamentplayer.cpp
	projects/lib/src/solution.cpp
	projects/lib/src/solutionbook.cpp
	projects/lib/src/solver.cpp
	projects/lib/src/positioninfo.cpp

	projects/lib/components/json/src/jsonparser.cpp
	projects/lib/components/json/src/jsonserializer.cpp

	projects/lib/3rdparty/fathom/src/tbprobe.c

	projects/lib/res/eco/eco.qrc
)

if(WIN32)
	target_sources(lib PRIVATE
		projects/lib/src/engineprocess_win.cpp
		projects/lib/src/pipereader_win.cpp
	)
endif()

target_compile_definitions(lib
	#PRIVATE UNICODE
	PRIVATE ANTI
	PRIVATE USE_POPCNT
	PRIVATE USE_PEXT
)

target_link_libraries(lib
	$<TARGET_OBJECTS:dictzip>
)

target_compile_definitions(lib PUBLIC LIB_EXPORT=)
target_compile_definitions(lib PUBLIC SOLVER_VERSION="${SOLVER_VERSION}")
target_compile_definitions(lib PUBLIC CUTECHESS_VERSION="${CUTECHESS_VERSION}")
target_compile_definitions(lib PRIVATE Z_PREFIX)
set_target_properties(lib PROPERTIES OUTPUT_NAME libsolver)

target_include_directories(lib PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/projects/lib/src>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/projects/lib/src/board>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/projects/lib/3rdparty/fathom/src>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/projects/lib/components/json/src>
)

target_link_libraries(lib Qt::Core)
if(Qt6_FOUND)
	target_link_libraries(lib Qt::Core5Compat)
endif()

add_executable(gui
	projects/gui/src/boardview/graphicspiece.cpp
	projects/gui/src/boardview/boardview.cpp
	projects/gui/src/boardview/boardscene.cpp
	projects/gui/src/boardview/graphicsboard.cpp
	projects/gui/src/boardview/graphicspiecereserve.cpp
	projects/gui/src/boardview/piecechooser.cpp

	projects/gui/src/flowlayout/flowlayout.cpp

	projects/gui/src/mainwindow.cpp
	projects/gui/src/gameviewer.cpp
	projects/gui/src/pgntoken.cpp
	projects/gui/src/settingsdlg.cpp
	projects/gui/src/solutiondlg.cpp
	projects/gui/src/importdlg.cpp
	projects/gui/src/solutionsmodel.cpp
	projects/gui/src/solutionitem.cpp
	projects/gui/src/main.cpp
	projects/gui/src/gametabbar.cpp
	projects/gui/src/movenumbertoken.cpp
	projects/gui/src/movetoken.cpp
	projects/gui/src/movecommenttoken.cpp
	projects/gui/src/movelist.cpp
	projects/gui/src/evaluation.cpp
	projects/gui/src/results.cpp
	projects/gui/src/titlewidget.cpp
	projects/gui/src/solutionswidget.cpp
	projects/gui/src/cutechessapp.cpp
	projects/gui/src/plaintextlog.cpp
	projects/gui/src/pathlineedit.cpp

	projects/gui/ui/settingsdlg.ui
	projects/gui/ui/solutiondlg.ui
	projects/gui/ui/importdlg.ui
	projects/gui/ui/solutionswidget.ui
	projects/gui/ui/evaluationwidget.ui
	projects/gui/ui/movelistwidget.ui
	projects/gui/ui/resultswidget.ui
	projects/gui/ui/titlewidget.ui

	projects/gui/3rdparty/qcustomplot/qcustomplot.cpp

	projects/gui/res/chessboard/chessboard.qrc
	projects/gui/res/icons/icons.qrc
	projects/gui/res/styles/styles.qrc
)

if(WIN32)
	target_sources(gui PRIVATE
		projects/gui/res/icons/cutechess_win.rc
	)
endif()

set_target_properties(gui PROPERTIES OUTPUT_NAME solver)
set_target_properties(gui PROPERTIES AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/projects/gui/ui)
set_target_properties(gui PROPERTIES WIN32_EXECUTABLE ON)

target_include_directories(gui PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/projects/gui/3rdparty/qcustomplot>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/projects/gui/src>
)

target_link_libraries(gui Qt::Core Qt::Widgets Qt::Gui Qt::Concurrent Qt::Svg Qt::PrintSupport)
if(Qt6_FOUND)
	target_link_libraries(gui Qt::Core5Compat)
endif()
target_link_libraries(gui lib)

if(WITH_TESTS)
	macro(add_unit_test test_name test_src)
		add_executable(test_${test_name} ${test_src})
		target_link_libraries(test_${test_name} Qt::Core Qt::Concurrent Qt::Test)
		target_link_libraries(test_${test_name} lib)
		target_compile_definitions(test_${test_name} PRIVATE CUTECHESS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/projects/lib/tests/data")
		add_test(test_${test_name} test_${test_name})
	endmacro(add_unit_test)

	add_unit_test(chessboard projects/lib/tests/chessboard/tst_board.cpp)
	add_unit_test(tb projects/lib/tests/tb/tst_tb.cpp)
	add_unit_test(sprt projects/lib/tests/sprt/tst_sprt.cpp)
	add_unit_test(mersenne projects/lib/tests/mersenne/tst_mersenne.cpp)
	add_unit_test(tournamentplayer projects/lib/tests/tournamentplayer/tst_tournamentplayer.cpp)
	add_unit_test(tournamentpair projects/lib/tests/tournamentpair/tst_tournamentpair.cpp)
	add_unit_test(polyglotbook projects/lib/tests/polyglotbook/tst_polyglotbook.cpp)
	add_unit_test(xboardengine projects/lib/tests/xboardengine/tst_xboardengine.cpp)
	if(WIN32)
		add_unit_test(pipereader projects/lib/tests/pipereader/tst_pipereader.cpp)
	endif()
endif()

install(TARGETS gui DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
install(FILES dist/linux/cutechess.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT Runtime)
install(FILES projects/gui/res/icons/cutechess_256x256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/application/256x256/apps/ RENAME cutechess.png COMPONENT Runtime)
set_property(TARGET lib PROPERTY CXX_STANDARD 17)
set_property(TARGET gui PROPERTY CXX_STANDARD 17)
install(FILES docs/cutechess-engines.json.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5/ COMPONENT Documentation)
