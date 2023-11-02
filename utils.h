
#pragma once
#include <string>
#include <vector>

struct message {
	std::string command;
	std::string param;
};

std::string createMsg(const std::string& msg);

std::vector<std::string> split_string(const std::string& str, char delim = ';');

message parseMsg(const char *buffer);