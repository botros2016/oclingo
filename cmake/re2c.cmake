MACRO(RE2C SRC DST VAR)
    IF(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}.cpp")
        MESSAGE(${CMAKE_CURRENT_SOURCE_DIR}/${SRC}.r2c)
        IF(NOT RE2C_FOUND)
            FIND_PACKAGE(RE2C REQUIRED)
        ENDIF(NOT RE2C_FOUND)
        ADD_CUSTOM_COMMAND(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${DST}.cpp
            COMMAND ${RE2C_EXECUTABLE} -o ${CMAKE_CURRENT_BINARY_DIR}/${DST}.cpp ${CMAKE_CURRENT_SOURCE_DIR}/${SRC}.r2c
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${SRC}.r2c
        )
        SET(${VAR} ${${VAR}} "${CMAKE_CURRENT_BINARY_DIR}/${DST}.cpp")
    ENDIF()
ENDMACRO()