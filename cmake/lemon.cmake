MACRO(LEMON SRC DST VAR)
    IF(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}.cpp" AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}.h")
	MESSAGE("***********cmake -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/${DST}.c ${CMAKE_CURRENT_BINARY_DIR}/${DST}.cpp")
        ADD_CUSTOM_COMMAND(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lempar.c
            COMMAND cmake -E create_symlink ${CMAKE_SOURCE_DIR}/lemon/lempar.c ${CMAKE_CURRENT_BINARY_DIR}/lempar.c
            MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/lemon/lempar.c
        )
        ADD_CUSTOM_COMMAND(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${DST}.c ${CMAKE_CURRENT_BINARY_DIR}/${DST}.cpp ${CMAKE_CURRENT_BINARY_DIR}/${DST}.h
            COMMAND cmake -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/${SRC}.y ${CMAKE_CURRENT_BINARY_DIR}/${DST}.y
            COMMAND cmake -E chdir ${CMAKE_CURRENT_BINARY_DIR} ${EXECUTABLE_OUTPUT_PATH}/lemon -q ${DST}.y
            COMMAND cmake -E create_symlink ${CMAKE_CURRENT_BINARY_DIR}/${DST}.c ${CMAKE_CURRENT_BINARY_DIR}/${DST}.cpp
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${SRC}.y
            DEPENDS lemon ${CMAKE_CURRENT_BINARY_DIR}/lempar.c
        )
        SET(${VAR} ${${VAR}} ${CMAKE_CURRENT_BINARY_DIR}/${DST}.cpp)
    ENDIF()
ENDMACRO()
