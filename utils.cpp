
#include <sstream>
#include <string.h>
#include <iostream>

#include "utils.h"

//Create message
	
std::string createMsg(const std::string& msg)
{
	std::string newMsg = msg;
	// Remove newline if present
	if (!newMsg.empty() && newMsg.back() == '\n') 
	{
        newMsg.pop_back();
    }

	newMsg = "\x02" + newMsg + "\x03";

	return newMsg;
}

std::vector<std::string> split_string(const std::string& str, char delim)
{
	std::vector<std::string> result;

	std::istringstream ss(str);
	while( ss.good() )
	{
		std::string substr;
		std::getline( ss, substr, delim );
		result.push_back( substr );
	}

	return result;
}

message parseMsg(const char *buffer)
{
	message result;

	// message should be <STX>command,parameters<ETX>
	// comma and parameters seems to be optional
	const char *start = strchr(buffer, '\x02');
	const char *comma = strchr(buffer, ',');
	const char *end = strchr(buffer, '\x03');

	if (start == NULL || end == NULL || end <= start)
	{
		std::cout << "Message not formatted correctly" << std::endl;
		return result;
	}

	if (comma == NULL) {
		// no argument
		result.command = std::string(start + 1, end - start - 1);
	} else {
		if ( (start >= comma) || (end <= comma) ) {
			std::cout << "Message not formatted correctly" << std::endl;
			return result;
		}
		result.command = std::string(start + 1, comma - start - 1);
		result.param = std::string(comma + 1, end - comma - 1);
	}


	return result;
}