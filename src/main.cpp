// ========================================================================
//
// The Forth interpreter-compiler by Prof. Boguslaw Cyganek (C) 2021
//
// The software is supplied as is and for educational purposes
// without any guarantees nor responsibility of its use in any application. 
//
// ========================================================================



#include "Interfaces.h"

#include "FiberRoutines.h"
#include "ESP32_config.h"
#include <numbers>


#include <cassert>
#include <memory>

//template < typename S, typename T >
//[[nodiscard]] constexpr S BlindValueReInterpretation_UBSafe( T t_val )
//{
	// These can lead to UB ...
	//if constexpr ( sizeof( S ) > sizeof( T ) )
	//	return ( * reinterpret_cast< S * >( & t_val ) ) & ( ( 1ull << sizeof( T ) * 8 ) - 1 );		// cut out excessive value (i.e. clear the bits from MSB up to bit_len( T )
	//else
	//	return * reinterpret_cast< S * >( & t_val );

//	return * std::start_lifetime_as< S >( & t_val );
//}


void ConverTest()
{
	constexpr BCForth::CellType	kCellVal { 0x0123456789ABCDEF };

	std::byte b = BCForth::BlindValueReInterpretation< std::byte >( kCellVal );

	//assert( b == 0xEF );		// won't compile
	assert( std::to_integer< unsigned int >( b ) == 0xEF );

	BCForth::CellType tmp { kCellVal };
	assert( tmp == kCellVal );


	tmp = BCForth::BlindValueReInterpretation< BCForth::CellType >( b );
	assert( tmp == 0x00000000000000EF );

	// ---
	//std::byte bb = BlindValueReInterpretation_UBSafe< std::byte >( kCellVal );
	//assert( std::to_integer< unsigned int >( bb ) == 0xEF );


}


void MemTest()
{
	std::cout << "sizeof( BCForth::Name ) = "				<< sizeof( BCForth::Name )					<< std::endl;
	std::cout << "sizeof( BCForth::DebugFileInfo ) = " << sizeof( BCForth::DebugFileInfo )		<< std::endl;
	std::cout << "sizeof( BCForth::Token ) = "			<< sizeof( BCForth::Token )				<< std::endl;
}





extern "C" void app_main(void){
	ESP32::register_spiffs();
	ESP32::configure();
	BCForth::Run();
	ESP32::unregister_spiffs();
}


