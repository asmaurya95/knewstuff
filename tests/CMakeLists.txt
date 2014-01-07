project(knewstuff2tests)
set(CMAKE_AUTOMOC TRUE)

include(ECMMarkAsTest)

set(CMAKE_SKIP_RPATH TRUE)

MACRO(KNEWSTUFF_UNIT_TESTS)
       FOREACH(_testname ${ARGN})
               add_executable(${_testname} ${_testname}.cpp)
               add_test("knewstuff2-${_testname}" ${_testname})
               ecm_mark_as_test(${_testname})
               target_link_libraries(${_testname} knewstuff2 Qt5::Xml Qt5::Test)
               set_target_properties(${_testname} PROPERTIES COMPILE_FLAGS -DKNSSRCDIR="\\"${CMAKE_CURRENT_SOURCE_DIR}/\\""\ -DKNSBUILDDIR="\\"${CMAKE_CURRENT_BINARY_DIR}\\"")
       ENDFOREACH(_testname)
ENDMACRO(KNEWSTUFF_UNIT_TESTS)

MACRO(KNEWSTUFF_EXECUTABLE_TESTS)
       FOREACH(_testname ${ARGN})
               add_executable(${_testname} ${_testname}.cpp)
               ecm_mark_as_test(${_testname})
               target_link_libraries(${_testname} knewstuff2 Qt5::Xml Qt5::Test)
               set_target_properties(${_testname} PROPERTIES COMPILE_FLAGS -DKNSSRCDIR="\\"${CMAKE_CURRENT_SOURCE_DIR}/\\""\ -DKNSBUILDDIR="\\"${CMAKE_CURRENT_BINARY_DIR}\\"")
       ENDFOREACH(_testname)
ENDMACRO(KNEWSTUFF_EXECUTABLE_TESTS)

##########################

# unit tests

if(NOT KDE_NO_DEPRECATED)

#   KNEWSTUFF_UNIT_TESTS(
#     knewstuffauthortest
#     knewstuffentrytest
#     testTranslatable
#   )
#
#   KNEWSTUFF_EXECUTABLE_TESTS(
#    knewstuff2_test
#    knewstuff2_download
#    knewstuff2_standard
#    knewstuff2_cache
#   )

##########################

#set(kdxspreview_SRCS
#   kdxspreview.cpp
#   kdxsview.cpp
#)

#add_definitions(-DKNSSRCDIR=\\"${CMAKE_CURRENT_SOURCE_DIR}\\")
#set(CMAKE_SKIP_RPATH TRUE)

#kde4_add_executable(kdxspreview TEST ${kdxspreview_SRCS})

#target_link_libraries(kdxspreview knewstuff2)
#install(FILES kdxspreviewrc DESTINATION ${CONFIG_INSTALL_DIR})
endif()
