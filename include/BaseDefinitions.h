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
#include <iomanip>
#include <string>
#include <vector>
#include <regex>
#include <map>
#include <filesystem>
#include <ranges>
#include <string_view>


namespace BCForth
{


	using std::cout, std::endl;
	using std::string;
	using std::vector;


	using namespace std::string_view_literals;	// Use this to avoid "MSVC: warning C4455: 'operator ""sv': literal suffix identifiers that do not start with an underscore are reserved"
	//using std::operator""sv;


	using CellType	= size_t;
	using RawByte	= unsigned char;
	using Char		= char;

	using size_type = size_t;

	using SignedIntType = long long;		// it is good and efficient to have the SignedIntType 
											// the same length as the basic CellType (otherwise e.g. address
											// arithmetic causes address crop to 4 bytes if int is 4, etc.)
	static_assert( sizeof( CellType ) == sizeof( SignedIntType ) );

	using FloatType		= double;
	static_assert( sizeof( CellType ) == sizeof( FloatType ) );


	constexpr auto CellTypeSize = sizeof( CellType );


	constexpr auto FORTH_IS_CASE_INSENSITIVE { true };		// set to false to make Forth case sensitive (anyway, all built-in words are uppercase)


	template< typename T >
	constexpr T kTrue = T( 1 );  

	template< typename T >
	constexpr T kFalse = T( 0 );  


	constexpr CellType kBoolTrue	= static_cast< CellType >( kTrue< bool > );
	constexpr CellType kBoolFalse	= static_cast< CellType >( kFalse< bool > );



	enum EIntCompBase { kBin = 2, kOct = 8, kDec = 10, kHex = 16 };


	// 8 kB for the PAD temporary storage area
	inline const size_type k_PAD_Size { 8 * 1024 };

	// Each CompoWord stores a std::vector with its words - this is the initial size to reserve for these vectors
	constexpr size_type kCompoWord_VecInitReserveSize { 16 };		




	using Name = std::string;			// maybe std::wstring in the future?


	using Letter = Name::value_type;


	constexpr	Letter	kSpace		{ ' ' };
	constexpr	Letter	kTab			{ '\t' };
	constexpr	auto		kCR			{ "\n"sv };

	constexpr	Letter kColon		{ ':' };
	constexpr	Letter kSemColon	{ ';' };


	constexpr	Letter kLeftParen		{ '(' };
	constexpr	Letter kRightParen	{ ')' };
	constexpr	Letter kBackSlash		{ '\\' };


	constexpr auto kBlanks { " \t\n"sv };


	const std::regex kBlanksRe { "[[:s:]]+" }; // any whitespace, one or more times

	constexpr auto					kDotQuote	{ ".\""sv };
	constexpr	const Letter	kQuote		{ '\"' };

	constexpr auto		kAbortQuote	{ "ABORT\""sv };
	constexpr auto		kCommaQuote	{ ",\""sv };
	constexpr auto		kSQuote		{ "S\""sv };
	constexpr auto		kCQuote		{ "C\""sv };

	constexpr	const Letter kPlus			{ '+' };



	constexpr auto		kDEBUGGER		{ "DEBUGGER"sv };		// a word that takes on two values: "ON" or "OFF" to turn debugger on and off, respectively
	constexpr auto		kON				{ "ON"sv };
	constexpr auto		kOFF				{ "OFF"sv };
	constexpr auto		kDebugFileName	{ "DebugFileName"sv };	// a word that defines a string constant with a debug file name, e.g.
																				// : DebugFileName S" BCForthDebugInfoFile.txt" ;
																				// This can be redefined by a user
	constexpr auto		kDefaultDebugFileName	{ "BCForthDebugInfoFile.txt"sv };


	constexpr auto		kFIND				{ "FIND"sv };
	constexpr auto		kTick				{ "'"sv };
	constexpr auto		kTO				{ "TO"sv };
	constexpr auto		kCHAR				{ "CHAR"sv };
	constexpr auto		kCREATE			{ "CREATE"sv };
	constexpr auto		kB_CREATE_B		{ "[CREATE]"sv };

	constexpr auto		kIF				{ "IF"sv };
	constexpr auto		kELSE				{ "ELSE"sv };
	constexpr auto		kTHEN				{ "THEN"sv };
	constexpr auto		kDO				{ "DO"sv };
	constexpr auto		kQDO				{ "?DO"sv };
	constexpr auto		kLOOP				{ "LOOP"sv };
	constexpr auto		kPLOOP			{ "+LOOP"sv };
	constexpr auto		kI					{ "I"sv };
	constexpr auto		kJ					{ "J"sv };
	constexpr auto		kBEGIN			{ "BEGIN"sv };
	constexpr auto		kAGAIN			{ "AGAIN"sv };
	constexpr auto		kWHILE			{ "WHILE"sv };
	constexpr auto		kREPEAT			{ "REPEAT"sv };
	constexpr auto		kUNTIL			{ "UNTIL"sv };				
	constexpr auto		kEXIT				{ "EXIT"sv };
	constexpr auto		kCASE				{ "CASE"sv };
	constexpr auto		kOF				{ "OF"sv };
	constexpr auto		kENDOF			{ "ENDOF"sv };
	constexpr auto		kENDCASE			{ "ENDCASE"sv };
	constexpr auto		kB_TICK_B		{ "[']"sv };
	constexpr auto		kLB				{ "["sv };
	constexpr auto		kRB				{ "]"sv };				
	constexpr auto		kPOSTPONE		{ "POSTPONE"sv };
	constexpr auto		kLITERAL			{ "LITERAL"sv };
	constexpr auto		kDOES_G			{ "DOES>"sv };
	constexpr auto		kB_CHAR_B		{ "[CHAR]"sv };
	constexpr auto		kCO_RANGE		{ "CO_RANGE"sv };      // the only one limitation is the only one delimiter char here
	constexpr auto		kCO_FIBER		{ "CO_FIBER"sv };      // the only one limitation is the only one delimiter char here
	constexpr auto		kIMMEDIATE		{ "IMMEDIATE"sv };				





	using Names = std::vector< Name >;



	////////////////////////////////////////////////////////////////
	// Auxiliary functions


	// Does simple re-interpretation rather than a value conversion.
	template < typename S, typename T >
	[[nodiscard]] constexpr S BlindValueReInterpretation( T t_val )
	{
		if constexpr ( sizeof( S ) > sizeof( T ) )
			return ( * reinterpret_cast< S * >( & t_val ) ) & ( ( 1ull << sizeof( T ) * 8 ) - 1 );		// cut out excessive value (i.e. clear the bits from MSB up to bit_len( T )
		else
			return * reinterpret_cast< S * >( & t_val );
	}
	// std::bit_cast will not work since it requires: sizeof(To) == sizeof(From)
	// start_lifetime_as
	// It means that ():
	// "an object should be created at the given memory location without running any initialisation code"



	[[nodiscard]] auto Letter_2_Name( const Letter letter )
	{
		return Name( sizeof( Letter ), letter );
	}


	[[nodiscard]] auto ContainsSubstrAt( const Name & n, const Name & substr )
	{
		return n.find( substr );
	}

	[[nodiscard]] auto ContainsSubstrAt( const Name & n, const Letter letter )
	{
		return ContainsSubstrAt( n, Letter_2_Name( letter ) );
	}


	[[nodiscard]] auto SplitAt( const Name & n, auto pos )
	{
		return std::make_tuple< Name, Name >( Name( n.begin(), n.begin() + pos ), Name( n.begin() + pos, n.end() ) );
	}




	// A helper to push a cell type object into the raw byte array
	template < typename V >
	constexpr void PushValTo( std::vector< RawByte > & byte_ar, V val )
	{
		for( auto i { 0 }; i < sizeof( val ); ++ i )
			byte_ar.push_back( val & 0xFF ), val >>= 8;
	}


	template < typename A, typename B >
	constexpr bool CheckMatch( A && a, B && b )
	{
		if constexpr( FORTH_IS_CASE_INSENSITIVE )

			return std::equal(	a.begin(), a.end(),
										b.begin(), b.end(),
										[] ( const auto x, const auto y ) { return std::toupper( x ) == std::toupper( y ); } );

		else

			return std::equal(	a.begin(), a.end(),
										b.begin(), b.end() );

	}

	constexpr [[nodiscard]] auto ToUpper( Name n )
	{
		return n | std::ranges::views::transform( [] ( auto c ) { return std::toupper( c ); } ) | std::ranges::to< Name >();
	}


	// Custom error type - the message will be displayed to the user
	class [[nodiscard]] ForthError : public std::runtime_error
	{
		private:

			bool fClearStacks { true };

		public:

			[[nodiscard]] bool MustClearStacks( void ) const { return fClearStacks; }

		public:

			ForthError( const std::string & what_arg, bool clear_stacks = true ) : runtime_error( what_arg ), fClearStacks( clear_stacks ) {}
			ForthError( const char * what_arg, bool clear_stacks = true ) : runtime_error( what_arg ), fClearStacks( clear_stacks ) {}
	};



	// ================
	// DEBUGGIN objects


	#define DEBUG_ON	1


	// When debugging, instead of passing a stream of bare strings, we need
	// more detailed information, such as 
	// - a token name
	// - its position in the (text) source file
	// - a name of that file (index, not to copy text)


	// Strong type
	struct  [[nodiscard]] SourceFileIndex
	{
		static const short kSentinelVal { -1 };

		short fIndex { kSentinelVal };

		static SourceFileIndex GetUniqueFileId()
		{
			static short id { 0 };
			return SourceFileIndex { id ++ };
		}

		friend auto operator <=> (const SourceFileIndex &, const SourceFileIndex &) = default;

	};

	constexpr SourceFileIndex kSourceFileIndexSentinel { SourceFileIndex::kSentinelVal };


#if DEBUG_ON




	using SourceFilesMap = std::map< SourceFileIndex, std::filesystem::path >;

		
	using LnCol = std::tuple< short, short >;


	
	struct DebugFileInfo
	{
		LnCol						fSourceFile_LnCol {};
		SourceFileIndex		fSourceFile_Index {};
	};



#else

	struct DebugFileInfo
	{
	};


#endif


	// The C++20 attribute [[no_unique_address]] can help the compiler to eliminate a member if its class is empty.
	// In such a case, the members will share the same address.
	// This falls into the broader subject called Empty Base Class Optimization (EBO).
	// Explanation of [[no_unique_address]] and [[msvc::no_unique_address]]
	// https://www.cppstories.com/2021/no-unique-address/ 
	//
	// If the member has an empty class, the compiler can eliminate it as if it were not there at all.
	// 
	// https://en.cppreference.com/w/cpp/language/attributes/no_unique_address
	// NOTE: [[no_unique_address]] is ignored by MSVC even in C++20 mode; instead, [[msvc::no_unique_address]] is provided. 
	struct Token 
	{
												Name				fName {};				// token string 
		[[msvc::no_unique_address]]	DebugFileInfo	fDebugFileInfo {};	// token position (line, col), file index
	};

	using TokenStream = std::vector< Token >;



}	// The end of the BCForth namespace


