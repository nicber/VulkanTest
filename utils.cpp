#include "utils.h"
#include <fstream>

std::vector<uint8_t>
read_file( const char *path )
{
	std::ifstream file{ path, std::ios::ate | std::ios::binary };
	std::vector<uint8_t> result;
	result.resize( ( unsigned long ) file.tellg() );
	file.seekg( 0 );
	file.read( ( char * ) result.data(), result.size() );
	file.close();
	return result;
}
