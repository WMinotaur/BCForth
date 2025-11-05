cmake_minimum_required( VERSION 3.13.0 )

target_sources( ${PROJECT_NAME}
    PUBLIC
        Interfaces.h
	Tokenizer.h
)

target_include_directories( ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR} )






