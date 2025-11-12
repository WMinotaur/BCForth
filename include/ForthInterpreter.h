// ========================================================================
//
// The Forth interpreter-compiler by Prof. Boguslaw Cyganek (C) 2021
//
// The software is supplied as is and for educational purposes
// without any guarantees nor responsibility of its use in any application. 
//
// ========================================================================



#pragma once


#include <iostream>
#include <format>
//#include <fmt/format.h>

#include "Forth.h"
#include "SystemWords.h"
#include "StructWords.h"



namespace BCForth
{




	// Forth interpreter (parser)
	class TForthInterpreter : public TForth
	{

	protected:

		using Base = TForth;

		using Base::fWordDict;



	protected:

		// The output stream used by all output words, such as EMIT or TYPE
		std::ostream & fOutStream { std::cout };

	public:

		[[nodiscard]] auto & GetOutStream( void ) { return fOutStream; }


	protected:


		const std::regex kIntVal	{ R"(([+-]?\d+))" };
		const std::regex kHexIntVal { R"((0[x|X][\da-fA-F]+))" };		// use the C++ syntax

		const std::regex kFloatPtVal { R"([+-]?(\d+[.]\d*([eE][+-]?\d+)?|[.]\d+([eE][+-]?\d+)?))" };		// always require a dot inside


		// A language cathegory in a word definition can be as follows:
		// - a word's name
		// - a reference to another word, i.e. already present in the dictionary
		// - a numerical value, either a character, an integer or the floating-point

		// There are built in words which need to be pre-defined by the Forth initializing code.
		// These are:
		// - basic arithmetic operations, +, -, *, /, */, 1+, 1-, etc.
		// - basic logical operators &, |, ^, not
		// - DUP, DROP, OVER, ROT and their "2" versions
		// - moves between the stacks: >R, <R, etc.
		// - IO operations: . (dot), .", "
		// - CONSTANT and VARIABLE

	protected:

		RawByte ReadVariable( const Name & variable_name )
		{
			if( const auto base_word_entry = GetWordEntry( variable_name ) )												// if exists, variable_name is a Forth's variable
				if( auto * we = dynamic_cast< CompoWord< TForth > * >( (*base_word_entry)->fWordUP.get() ) )			// each Forth's word contains a CompoWord
					if( auto & compo_vec = we->GetWordsVec(); compo_vec.size() > 0 )											// The CompoWord should contain sub-words
						if( auto * byte_arr = dynamic_cast< RawByteArray< Base > * >( compo_vec[ 0 ] ) )					// For the variable this has to be RawByteArray
								return byte_arr->GetContainer()[ 0 ];


			return RawByte();			// this is the default value
		}


	public:

		EIntCompBase ReadTheBase( void ) override
		{
			return ReadVariable( "BASE" ) == 16 ? EIntCompBase::kHex : EIntCompBase::kDec;
		}

	protected:

		[[nodiscard]] auto Word_2_Integer( const Name & word )
		{
			try
			{
				return std::stoll( word, nullptr, ReadTheBase() );
			}
			catch( ... )
			{
				throw ForthError( "wrong format of the integer literal" );
			}
		}


		[[nodiscard]] bool IsInteger( const Name & n, EIntCompBase expected_base )
		{
			switch( expected_base )
			{
				case kDec: return std::regex_match( n, kIntVal );
				case kHex: return std::regex_match( n, kHexIntVal );
				default: assert( ! "not supported formats" ); break;
			}

			return false;
		}


		[[nodiscard]] bool IsFloatingPt( const Name & n )
		{
			return std::regex_match( n, kFloatPtVal );
		}



		template < typename T >
		void Erase_n_First_Words( T & ns, const Names::size_type to_remove )
		{
			ns.erase( ns.begin(), ns.begin() + to_remove );
		}


	protected:


		[[nodiscard]] auto ExtractTextFromTokenUpToSubstr( const Name & n, const Name::size_type pos, const Letter letter )
		{
			assert( n.length() > pos && n[ pos ] == letter );
			return n.substr( 0, pos );
		}



		// Allows for recursive (nested) enclosings
		[[nodiscard]] auto CollectTextUpToTokenContaining( /*Names*/TokenStream & ns, const Letter enter_letter, const Letter close_letter )
		{
			Name str;

			bool internal_mode { false };			// if false, then the closing letter causes exit
													// if true then disregard the closing letter and continue collection (for nested parenthesis, etc.)

			while( ns.size() > 0 )
			{
				const auto token { ns[ 0 ].fName };		// at each iteration take the first word
				Erase_n_First_Words( ns, 1 );		// immediately, remove from the stream

				if( const auto pos = ContainsSubstrAt( token, enter_letter ) ; pos != Name::npos )
				{
					// If here, then we encountered the nested definition (i.e. parenthesis inside parenthesis), so we need to match them
					if( const auto pos = ContainsSubstrAt( token, close_letter ); pos == Name::npos )
					{	
						// no closing symbol yet
						internal_mode = true;		// change internal mode only on unbalanced symbols
						str += token + kSpace;		// spaces were lost by the lexer, so we need to add it here
					}
					else
					{
						// a closing symbol found - split, left attach, right treat as a new token
						const auto & [ s0, s1 ] = SplitAt( token, pos + 1 );		
						str += s0;
						//ns.insert( ns.begin(), s1 );	// insert new token to the stream
						ns.insert( ns.begin(), Token { s1/*, DebugFileInfo()*/ } );	// insert new token to the stream
					}

				}
				else
				{

					if( const auto pos = ContainsSubstrAt( token, close_letter ) ; pos != Name::npos )
					{

						if( internal_mode )
						{
							internal_mode = false;
						}
						else
						{
							// " can be glued to the end of a text, so get rid of it and exit
							str += ExtractTextFromTokenUpToSubstr( token, pos, close_letter );
							return std::make_tuple< bool, Name >( true, std::move( str ) );
						}
					}


					str += token + kSpace;	// spaces were lost by the lexer, so we need to add it here

				}

			}

			return std::make_tuple< bool, Name >( false, "" );		// something went wrong, no ending quotation
		}




	protected:


		// Process and consume the processed words (i.e. these will be erased from the input stream)
		// ns will be modified
		virtual void ProcessContextSequences( TokenStream & ns )
		{
			const auto kNumNames { ns.size() };
			if( kNumNames == 0 )
				return;

			auto & leadName { ns[ 0 ].fName };


			// e.g. FIND DROP
			if( CheckMatch( leadName, kFIND ) )
			{
				// There should be a following name for that word
				if( kNumNames <= 1 )
					throw ForthError( "Syntax missing word name" );

				if( auto word_entry = GetWordEntry( ns[ 1 ].fName ); word_entry )
					cout << "Word " << ns[ 1 ].fName << " found ==> ( " << ( * word_entry )->fWordComment << " )" << ( ( * word_entry )->fWordIsImmediate ? "\t\timmediate" : "" ) << endl;
				else
					cout << "Unknown word " << ns[ 1 ].fName << endl;

				Erase_n_First_Words( ns, 2 );
				return;
			}


			// e.g. ' DUP
			if( CheckMatch( leadName, kTick ) )
			{
				// There should be a following name for that variable
				if( kNumNames <= 1 )
					throw ForthError( "Syntax missing variable name" );

				Tick< TForth >( * this, ns[ 1 ].fName )();

				Erase_n_First_Words( ns, 2 );
				return;
			}



			// This one goes like this:
			// e.g. 234 TO CUR_FUEL
			if( CheckMatch( leadName, kTO ) )
			{
				// There should be a following name for that variable
				if( kNumNames <= 1 )
					throw ForthError( "Syntax missing variable name" );

				To< TForth >( * this, ns[ 1 ].fName )();

				Erase_n_First_Words( ns, 2 );
				return;
			}


			// Parse the following word and put ASCII code of its first char onto the stack
			if( CheckMatch( leadName, kCHAR ) )
			{
				// There should be a following name for that variable
				if( kNumNames <= 1 )
					throw ForthError( "Syntax CHAR should be followed by a text" );

				GetDataStack().Push( BlindValueReInterpretation< Char >( ns[ 1 ].fName[ 0 ] ) );

				Erase_n_First_Words( ns, 2 );
				return;
			}


			// Constructions to enter the counted string (i.e. len-chars), e.g.:
			// CREATE	AGH		,"	University of Science and Technology"
			if( CheckMatch( leadName, kCommaQuote ) )
			{
				// Just entered the number of tokens up to the closing "

				Erase_n_First_Words( ns, 1 );

				if( auto [ flag, str ] = CollectTextUpToTokenContaining( ns, Letter(), kQuote ); flag )
					CommaQuote< TForth >( * this, str )();		// create & call
				else
					throw ForthError( "no closing \" found for the opening ,\"" );

				return;
			}


			// CREATE DATA  100 CHARS ALLOT
			// CREATE CELL-DATA  100 CELLS ALLOT
			// CREATE TWOS 2 , 4 , 8 , 16 ,
			if( CheckMatch( leadName, kCREATE ) )
			{
				leadName = ns[ 0 ].fName = kB_CREATE_B;	// just exchange CREATE into [CREATE] and business as usual
			}

		}


		// The main entry to the Forth's INTERPRETER
		virtual void ExecuteWords( TokenStream && ns )
		{
			if( ns.size() == 0 )
				return;

			auto & word { ns[ 0 ].fName };


#if DEBUG_ON

			// -----------------
			// Debugger on & off
			if( CheckMatch( word, kDEBUGGER ) )
			{
				// Next thing to determine is "ON" or "OFF"
				if( ns.size() <= 1 )
					throw ForthError( "Missing 'ON' or 'OFF' in DEBUGGER command" );

				if( const auto & name = ns[ 1 ].fName; CheckMatch( name, kON ) )
					SetDebugMode( true );		
				else
					if( CheckMatch( name, kOFF ) )
						SetDebugMode( false );	
					else
						throw ForthError( "Missing 'ON' or 'OFF' in DEBUGGER command" );

				Erase_n_First_Words( ns, 2 );

				return;
			}
			// -----------------

			CallDebugWord( word, ns[ 0 ].fDebugFileInfo );				// otherwise, take the current token debug context

#endif // DEBUG_ON




			ProcessContextSequences( ns );			// moved here on Sept 13th '23 - ProcessContextSequences also returns on an empty ns

			// ProcessContextSequences can check length of ns
			if( ns.size() == 0 )
				return;

			word = ns[ 0 ].fName;


			if( IsInteger( word, ReadTheBase() ) )
			{
				GetDataStack().Push( Word_2_Integer( word ) );
				Erase_n_First_Words( ns, 1 );	// get rid of the already consumed word
				ExecuteWords( std::move( ns ) );
				return;
			}


			if( IsFloatingPt( word ) )
			{
				GetDataStack().Push( BlindValueReInterpretation< CellType >( stod( word ) ) );		// for now, later we need to devise something better for floats
				Erase_n_First_Words( ns, 1 );	// get rid of the already consumed word
				ExecuteWords( std::move( ns ) );
				return;
			}


			if( ProcessDefiningWord( word, ns ) )
			{
				Erase_n_First_Words( ns, 2 );	// get rid of the already consumed words
				ExecuteWords( std::move( ns ) );
				return;
			}


			// Check if a registered word and execute
			if( ExecWord( word ) )
			{
				Erase_n_First_Words( ns, 1 );	// get rid of the already consumed word
				ExecuteWords( std::move( ns ) );
				return;
			}
			else
			{
				throw ForthError( "unknown word - " + word, false );
			}

		}


	protected:


		// The one created with DOES>
		virtual bool ProcessDefiningWord( const Name & word_name, const TokenStream & ns )
		{
			// First, check if this is a defining word
			if( auto word_entry = GetWordEntry( word_name ); word_entry && (*word_entry)->fWordIsDefining )
			{
				if( auto * we = dynamic_cast< CompoWord< TForth > * >( (*word_entry)->fWordUP.get() ) )
				{

					if( auto & compo_vec = we->GetWordsVec(); compo_vec.size() == 1 )
					{
						if( auto * does_wrd = dynamic_cast< DOES< TForth > * >( compo_vec[ 0 ] ) )
						{
							// Ok, this is the DOES> word

							// There should be a following name for that variable
							if( ns.size() <= 1 )
								throw ForthError( "Syntax missing variable name for the defining word" );


							// Call the creation branch - this should leave 
							// (i) some values on the stack
							// (ii) new RawByteArray in the local repository due to CREATE
							( * does_wrd )();


							// The RawByteArray should be already in the fNodeRepo, so let's access it and verify its identity
							if( fNodeRepo.size() == 0 )
								throw ForthError( "missing CREATE action in the defining word" );	

							auto & array_wp = fNodeRepo[ fNodeRepo.size() - 1 ];		// let's access - this will be WordUP

							auto * arr_wrd = dynamic_cast< RawByteArray< TForth > * >( array_wp.get() );
							if( arr_wrd == nullptr )
								throw ForthError( "missing CREATE action in the defining word" );	


							// Create a new entry with connected behavioral branch
							auto definedWord { std::make_unique< CompoWord< TForth > >( * this ) };
							auto definedWordPtr { definedWord.get() };

							definedWordPtr->AddWord( arr_wrd );								// (1) Connect the RawByteArray word - whenever called it will leave the address of its data 
							if( ! IsEmpty( does_wrd->GetBehaviorNode() ) )
								definedWordPtr->AddWord( & does_wrd->GetBehaviorNode() );	// (2) Connect the behavioral branch, as already pre-defined in the defining word

							InsertWord_2_Dict( ns[ 1 ].fName, std::move( definedWord ), Name( kDOES_G ) + word_name, false, false, false, ns[ 1 ].fDebugFileInfo );		// Now we have fully created new word in the dictionary		

							return true;
						}


					}


				}
				
				
				assert( false );		// should not happen, all words are at least of CompoWord type or derived

			}

			return false;		// no, this is not a defining word 
		}


		// Destructor will be created automatically virtual due to declaration in the base class



	public:



		// Process a stream of tokens
		virtual void operator() ( TokenStream && ns )
		{
			ExecuteWords( std::move( ns ) );							
			CallDebugWord();				// otherwise, take the current token debug context
		}

	public:

		// It clears the data and return stacks - should be called in the catch 
		virtual void CleanUpAfterRunTimeError( bool must_clear_stacks )
		{
			if( must_clear_stacks )
				GetDataStack().clear(), GetRetStack().clear();	
		}


	public:

#if DEBUG_ON

		// -------------
		// DEBUG members


		[[nodiscard]] auto IsDebug()
		{
			return GetDebugMode();
		}


		Name GetDebugFileName()
		{
			// At first - invoke the word "DebugFileName"
			// This will leave addr and len on the stack			
			// Then create and return a string
			if( DataStack::value_type x {}, y {}; ExecWord( Name( kDebugFileName ) ) && fDataStack.Pop( y ) && fDataStack.Pop( x ) )
				return Name( reinterpret_cast< Letter * >( x ), y );
			else
				return Name( kDefaultDebugFileName );
		}

		void CallDebugWord( const Name & word_name = "", const DebugFileInfo & debug_file_info = DebugFileInfo() )
		{
			if( ! IsDebug() )
				return;

			auto [ lnCol, fIdx ] = debug_file_info;
			auto [ ln, col ] = lnCol;
			auto fileName = fIdx != kSourceFileIndexSentinel ? fSourceFilesMap[ fIdx ].string() : "";

			if( fileName.size() > 0 )
			{
				std::ofstream outDebugFile( GetDebugFileName() );
				if( outDebugFile.is_open() )
					outDebugFile << fileName << "\n" << ln << " " << col;
			}

			//fOutStream << "\nTo exec >> " << word_name << " @ (" << ln << "," << col << "," << fileName << ")" << "\nStack dump: ";
			fOutStream << std::format( "\nTo exec >> {}  @ ({},{})\nStack dump: ", word_name, ln, col, fileName );
			fOutStream << "(c) cont, (s) signd st.dump & cont, (d) unsignd st.dump & cont, (x) stop debug & cont, (a) abort: ";


			auto stack_dump_lambda = [ this ]( auto DispTypeDummy ) 
			{
				const auto ds { GetDataStack().data() };
				const auto base { ReadTheBase() };
				fOutStream << std::setbase( base ) << ( base == EIntCompBase::kDec ? std::noshowbase : std::showbase );
				std::transform( ds, ds + GetDataStack().size(),	std::ostream_iterator< decltype( DispTypeDummy ) >( fOutStream, " " ), 
										[] ( const auto & v ) { return BlindValueReInterpretation< decltype( DispTypeDummy ) >( v ); } );
				fOutStream << std::endl;
			};


			switch( char c {}; std::cin >> c, c )
			{
				case 's': case 'S':

					stack_dump_lambda( SignedIntType() );
					break;

				case 'd': case 'D':

					stack_dump_lambda( CellType() );
					break;

				case 'x': case 'X':

					SetDebugMode( false );
					break;			
					
				case 'a': case 'A':

					throw ForthError( "DEBUGGING aborted by a user" );
					break;

				default:
					break;
			}

 		}



		[[nodiscard]] auto	GetWordEntryAndNameFromWordAddress( const WordPtr p )
		{
			if( auto pos = std::find_if(	fWordDict.begin(), fWordDict.end(), 
													[ p ] ( const auto & m ) { return m.second.fWordUP.get() == p; } ); 
						pos != fWordDict.end() )
				return std::tuple( WordOptional( & pos->second ), pos->first );
			else 
				return std::tuple( WordOptional(), Name() );
		}


		[[nodiscard]] Name	GetNameFromWordAddress( const WordPtr p ) const
		{
			if( auto pos = std::find_if(	fWordDict.begin(), fWordDict.end(), 
													[ p ] ( const auto & m ) { return m.second.fWordUP.get() == p; } ); 
						pos != fWordDict.end() )
				return pos->first;
			else 
				return Name();
		}

	private:


		bool					fDebugMode_On { false };			// if true, then we are in the debug mode

		SourceFilesMap		fSourceFilesMap;

		//Name					fDebugFileName { "BCForthDebugInfoFile.txt" };		// a file to post debug information to the outside world


	public:

		[[nodiscard]] SourceFilesMap &	GetSourceFilesMap() { return fSourceFilesMap; }

		void							SetDebugMode( bool v ) { fDebugMode_On = v; }
		[[nodiscard]] bool		GetDebugMode() const { return fDebugMode_On; }


#else


		void CallDebugWord( const Name & word_name = "", const DebugFileInfo & debug_file_info = DebugFileInfo() )
		{
		}



#endif



	};




	template < typename Base >
	void CompoWord< Base >::operator () ( void )
	{
#if DEBUG_ON
		if( auto & theInterpreter = dynamic_cast< TForthInterpreter & >( GetForth() ); theInterpreter.IsDebug() && fWordsDebugInfoVec.size() == fWordsVec.size() )
		{
			if( const auto [ word_entry_opt, name ] { theInterpreter.GetWordEntryAndNameFromWordAddress( this ) }; word_entry_opt )
				theInterpreter.CallDebugWord( name, (*word_entry_opt)->fDebugFileInfo );


			for( int i {}; i < std::ssize( fWordsVec ); ++ i )
			{
				const auto op = fWordsVec[ i ];

				( * op )();

				theInterpreter.CallDebugWord( theInterpreter.GetNameFromWordAddress( op ), fWordsDebugInfoVec[ i ] );
			}

		}
		else
#endif // DEBUG_ON
		{
			for( const auto op : fWordsVec )
				( * op )();
		}
	}




}	// The end of the BCForth namespace






