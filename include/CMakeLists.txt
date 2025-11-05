cmake_minimum_required( VERSION 3.13.0 )

target_sources( ${PROJECT_NAME}
    PUBLIC
        BaseDefinitions.h
	Forth.h
	ForthCompiler.h
	ForthInterpreter.h
)

target_include_directories( ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR} )

add_subdirectory( Auxiliary )
add_subdirectory( Interfaces )
add_subdirectory( Modules )
add_subdirectory( Words )




