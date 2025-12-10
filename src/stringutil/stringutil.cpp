#include <algorithm>
#include <cctype>
#include <common/stringutil/stringutil.h>
#include <sstream>
std::vector<std::string> StringUtil::split(const std::string& str, char c)
{
	std::vector<std::string> result;
	std::istringstream iss(str);
	std::string token;
	while (std::getline(iss, token, c))
	{
		// std::transform(token.begin(), token.end(), token.begin(), ::toupper);// 全转大写
		result.push_back(token);
	}
	return result;
}
std::string StringUtil::trim(const std::string& str)
{

	std::string result;
	std::remove_copy(str.begin(), str.end(), std::back_inserter(result), ' ');
	return result;
}
std::string StringUtil::upper(const std::string& str)
{
	std::string res = str;
	for (auto& c: res)
	{
		c = std::toupper(static_cast<unsigned char>(c));
	}
	return res;
}
std::string StringUtil::trimEdge(const std::string& str)
{
	size_t first = str.find_first_not_of(' ');
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(' ');
	return str.substr(first, last - first + 1);
}
