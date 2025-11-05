cmake_minimum_required( VERSION 3.13.0 )

target_sources( ${PROJECT_NAME}
    PUBLIC
        AD_Measurement.txt
	Debug.txt
	Debug_line_counter.txt
	Debug2222.txt
	ErathoSieve.txt	
	ErathoSieveEx.txt
	Factorial.txt
	Fibo.txt
	ForthExamples.txt
	ForthExamples_2.txt
	if_else_test.txt
	Postpone.txt
	QuadEq.txt
	QuadEqVars.txt
	Sort_by_AI.txt
	test.txt
	variable.txt
	VectoredExecution.txt
)

target_include_directories( ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR} )






