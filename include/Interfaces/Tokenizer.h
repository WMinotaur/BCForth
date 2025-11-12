// ========================================================================
//
// The Forth interpreter-compiler by Prof. Boguslaw Cyganek (C) 2021
//
// The software is supplied as is and for educational purposes
// without any guarantees nor responsibility of its use in any application. 
//
// ========================================================================


#pragma once




#include "BaseDefinitions.h"









namespace BCForth
{



#if DEBUG_ON


// Wersja debugująca czytnika
class TForthReader_4_Debugging
{

	// Debug data
	SourceFileIndex	fSourceFileIndex { SourceFileIndex::kSentinelVal };


	int fTotalCharCounter { 0 };		// counts all chars from the moment the file was opened


public:

	TForthReader_4_Debugging( SourceFileIndex fIndx = kSourceFileIndexSentinel ) : fSourceFileIndex( fIndx ) 
	{
	}

protected:


	// Returns true if 1-to-the-left and 1-to-the-right is space or backspace
	bool IsSeparateSymbol( const Name & str, int p )
	{
		assert( p >= 0 );
		assert( p < std::ssize( str ) );
		Letter left { p > 0 ? str[ p-1 ] : kSpace };
		Letter right { p < std::ssize( str )-1 ? str[ p+1 ] : kSpace }; 
		return ( left == kSpace || left == kTab ) && ( right == kSpace || right == kTab );
	}


    Name StripEndingComment( Name & ln )
	{
		if( auto pos = ln.find( kBackSlash ); pos != Name::npos )
			ln.erase( ln.begin() + pos, ln.end() );

		return ln;
	}


public:


	// Read a line or lines and return the names
	// However, in the DEBUG mode the thing is we need to store the line numbers alongside with the tokens
	virtual TokenStream operator() ( std::istream & i )	
	{
		assert( sizeof( Token ) > sizeof( Token::fName ) );

		TokenStream		outTokenStream;

		Name ln;

		// Read:
		// - read one line 
		// - check if there is a defining colon :
		// - if there is :, then read all consecutive lines up to and including the ending semicolon ;
		bool enterDefinition { false };


		for( int lineCnt{ 0 };	( enterDefinition == false && lineCnt == 0	&& std::getline( i, ln ) )
											||
										( enterDefinition == true							&& std::getline( i, ln ) )
			
				; ++ lineCnt )
		{
			Name tokName;		// a current token



			bool skipCommentLine { false };
			for( int colCnt{ 0 }; colCnt < ln.size() && skipCommentLine == false; ++ colCnt, ++ fTotalCharCounter )
			{


				// Filter out all blanks but keep line and column count
				switch( auto c = ln[ colCnt ]; c )
				{
					[[likely]] default:
						tokName += c;

						if( colCnt == ln.size() - 1 )
						{
							// If the last valid char is just at newline
							int tokSize = static_cast< int >( tokName.size() );		
							assert( tokSize > 0 );			// for sure it is > 0
							outTokenStream.emplace_back( Token { std::move( tokName ), { { fTotalCharCounter + 1 - tokSize, tokSize }, fSourceFileIndex } } );
						}

						break;


					case kSpace:
					case kTab:

						if( int tokSize = static_cast< int >( tokName.size() ); tokSize > 0 )
						{
							assert( fTotalCharCounter >= tokSize );
							outTokenStream.emplace_back( Token { std::move( tokName ), { { fTotalCharCounter - tokSize, tokSize }, fSourceFileIndex } } );	// assuming no SSO in tokName
							tokName.clear();
						}

						break;		// a token is complete, jump out

					case kBackSlash:

						assert( ln.size() >= colCnt + 1 );
						fTotalCharCounter += static_cast< int >( ln.size() ) - colCnt - 1;		// actuall "-1" because the backslash is aready accounted for
						skipCommentLine = true;
						assert( tokName.size() == 0 );
						break;


					[[unlikely]] case kColon:

						// we allows words such as "BUFFER:"
						if( IsSeparateSymbol( ln, colCnt ) )
						{
                            if( !tokName.empty() )
                            {
                                outTokenStream.emplace_back( Token { std::move( tokName ), { { fTotalCharCounter - static_cast<int>(tokName.size()), static_cast<int>(tokName.size()) }, fSourceFileIndex } } );
                                tokName.clear();
                            }
							outTokenStream.emplace_back( Token { Letter_2_Name( kColon ), { { fTotalCharCounter, 1 }, fSourceFileIndex } } );
							enterDefinition = true;
						}
						else
						{
							tokName += c;
						}

						break;

					[[unlikely]] case kSemColon:

						if( IsSeparateSymbol( ln, colCnt ) )
						{
                             if( !tokName.empty() )
                            {
                                outTokenStream.emplace_back( Token { std::move( tokName ), { { fTotalCharCounter - static_cast<int>(tokName.size()), static_cast<int>(tokName.size()) }, fSourceFileIndex } } );
                                tokName.clear();
                            }
							assert( fTotalCharCounter > 0 );
							outTokenStream.emplace_back( Token { Letter_2_Name( kSemColon ), { { fTotalCharCounter, 1 }, fSourceFileIndex } } );
							enterDefinition = false;
						}
						else
						{
							tokName += c;						
						}

						break;

				}


			}

			fTotalCharCounter += 2;		// count a hidden new line

		}


		return outTokenStream; 

	}
};


using TForthReader = TForthReader_4_Debugging;


#else

// Wersja bez debugowania (oryginalna)
class TForthReader_Release
{

public:

	virtual ~TForthReader_Release() = default;

protected:


	Name StripEndingComment( Name & ln )
	{
		if( auto pos = ln.find( kBackSlash ); pos != Name::npos )
			ln.erase( ln.begin() + pos, ln.end() );

		return ln;
	}


public:


	virtual TokenStream operator() ( std::istream & i )
	{

		Name lines;

		if( Name ln; std::getline( i, ln ) )
		{
			lines = StripEndingComment( ln );

			if( auto pos = ln.find_first_not_of( kBlanks ); pos != Name::npos && ln[ pos ] == kColon )
				while( ln.find( kSemColon, pos ) == Name::npos && std::getline( i, ln )  )
					lines += Name( kCR ) + StripEndingComment( ln );

		}

		assert( sizeof( Token ) == sizeof( Token::fName ) );

		auto tok_names = std::ranges::subrange( std::sregex_token_iterator( lines.cbegin(), lines.cend(), kBlanksRe, -1 ), std::sregex_token_iterator() );

		return	tok_names	| std::ranges::views::filter( [] ( const auto & s ) { return s.length() > 0; } )
									| std::ranges::views::transform( [] ( const auto & a ) { return Token { a }; } )
									| std::ranges::to< TokenStream >();

	}

};

using TForthReader = TForthReader_Release;


#endif



}	// The end of the BCForth namespace