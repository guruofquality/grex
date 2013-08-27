########################################################################
# Setup Boost
########################################################################
find_package(Boost COMPONENTS thread)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
list(APPEND GRAS_TOOL_LIBRARIES ${Boost_LIBRARIES})

########################################################################
## GrEx Network Blocks
########################################################################
GRAS_TOOL(
    SOURCES
        deserializer.cpp deserializer.xml
        serializer.cpp serializer.xml
        socket_message.cpp socket_message.xml
        tuntap.cpp tuntap.xml
    TARGET ex_network
    DIRECTORY ex
    COMPONENT ex
)

########################################################################
# Unit tests!
########################################################################
include(GRASTest)

#register unit tests
GRAS_ADD_TEST(serialize_test ${GRAS_TEST_PYEXE} ${CMAKE_CURRENT_SOURCE_DIR}/serialize_test.py)
GRAS_ADD_TEST(socket_test ${GRAS_TEST_PYEXE} ${CMAKE_CURRENT_SOURCE_DIR}/socket_test.py)
