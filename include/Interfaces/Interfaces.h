// ========================================================================
//
// The Forth interpreter-compiler by Prof. Boguslaw Cyganek (C) 2021
//
// The software is supplied as is and for educational purposes
// without any guarantees nor responsibility of its use in any application. 
//
// ========================================================================



#pragma once





#include "ForthCompiler.h"
#include "Modules.h"
#include "FP_Module.h"
#include "StringModule.h"
#include "CoreModule.h"
#include "RandModule.h"
#include "TimeModule.h"

#include "FiberRoutines.h"








namespace BCForth
{

	const Names kMenu_ExitWords		{ "BYE", "EXIT" };

	const Name  kMenu_FileLoadWord	{ "LOAD" };

	const Name  kMenu_Help			{ "HELP" };


	// This is called before the Forth interpreter
	// to check and take action on system words
	bool SystemProcessTokens( TForthCompiler & , const /*Names*/TokenStream & , bool & );


	const Name kWelcomeString { R"(==========================================
Welcome to the Forth interpreter-compiler
Written by Prof. Boguslaw Cyganek (C) 2023
==========================================
)" };


	const Name kHelpString { R"(----------------------------------------------------------
Load - loads & executes a text file
Exit, bye - to leave		
Words - prints a list of words in the dictionary
All operations on the stack in the Reverse Polish Notation							
----------------------------------------------------------)" };





	void Run( void )
	{
		std::cout << kWelcomeString;


		bool exit_flag { false };

		TForthCompiler	F_compiler;
		// /*TForthReader*/TForthReader_4_Debugging	theReader;



		// This is "a must"
		CoreEncodedWords()( F_compiler );
		CoreDefinedWords()( F_compiler );


		// Load extra modules - order matters (words depend on previous words)
		AuxStackWords()( F_compiler );
		FP_Module()( F_compiler );
		AuxTextModule()( F_compiler );
		StringModule()( F_compiler );
		RandomModule()( F_compiler );
		TimeModule()( F_compiler );

		FileForthModule( "../add_ons/AddOns.txt" )( F_compiler );	// new definitions can be added to the text file AddOns.txt



		do
		{

			try 
			{
				auto terminal_input_index { SourceFileIndex::GetUniqueFileId() };
#if DEBUG_ON
				F_compiler.GetSourceFilesMap()[ terminal_input_index ] = "Terminal";
				TForthReader_4_Debugging	theReader( terminal_input_index );
#else
				TForthReader theReader;
#endif

				while( ! exit_flag )
				{
					std::cout << "\nOK:" << std::endl;


					auto tokens { theReader( std::cin ) };

					if( SystemProcessTokens( F_compiler, tokens, exit_flag ) == false )
						F_compiler( std::move( tokens ) );	

					// --------------------------
					// Process coro scheduler
					ProcessCoros();

				}
			}
			catch( const ForthError & err )
			{
				F_compiler.CleanUpAfterRunTimeError( false );
				std::cerr << "\nError: " << err.what() << endl;		// after this kind of errors we proceed	
			}
			catch( const std::exception & std_excpt )
			{
				std::cerr << "\nStandard error: " << std_excpt.what() << endl;		// after this kind of errors we proceed	
			}
			catch( ... )
			{
				std::cerr << "\nSystem error, exiting ... " << endl;
				exit_flag = true;								// such error means finishing execution
			}

		}
		while( exit_flag == false );


	}



	// ----------------------------



	bool SystemProcessTokens( TForthCompiler & F_compiler, const TokenStream & ns, bool & exit_flag )
	{
		if( ns.size() == 0 )
			return false;			// nothing to do, exiting

		exit_flag = false;		// exit? not yet


		Name str = ns[ 0 ].fName;		
		if constexpr( FORTH_IS_CASE_INSENSITIVE )
			str = ToUpper( ns[ 0 ].fName );


		// -------------------------------------------------------------------------------
		// Some special words have to break the "normal" operation of the Forth's compiler
		if( std::find_if( kMenu_ExitWords.begin(), kMenu_ExitWords.end(), 
			[ & str ] ( const auto & ex_word ) { return str.find( ex_word ) != Name::npos; } ) != kMenu_ExitWords.end() )
		{
			std::cerr << "\nBye, bye to you, exiting ... " << endl;
			exit_flag = true;
			return true;
		}


		// ---------------------------------------------------------
		if( str.find( kMenu_FileLoadWord ) != Name::npos )
		{

			std::cout << "Enter path to the Forth code file [.txt]:\n";
			if( std::string forth_source; std::cin >> forth_source )
			{
				if( std::ifstream fs( forth_source ); fs )
				{
#if DEBUG_ON
					const auto file_index { SourceFileIndex::GetUniqueFileId() };
					F_compiler.GetSourceFilesMap() [ file_index ] = forth_source;
					for( TForthReader_4_Debugging fileReader( file_index ); fs; F_compiler( fileReader( fs ) ) ) 
						;
#else
					for( TForthReader fileReader; fs; F_compiler( fileReader( fs ) ) ) 
						;
#endif

					std::cout << "File processed OK\n" << endl;
				}
				else
				{
					std::cerr << "\nCannot open the file: " << forth_source << endl;		
				}
			}
			else
			{
				std::cerr << "\nWrong file path: " << forth_source << endl;		
			}

			return true;	// load processed, no matter if the file could be or not properly opened
		}

		// ---------------------------------------
		if( str.find( kMenu_Help ) != Name::npos )
		{
			std::cout << kHelpString << endl;
			return true;
		}

	
		return false;		// no system words, continue with the Forth interpreter
	}


	// ----------------------------


}	// The end of the BCForth namespace



