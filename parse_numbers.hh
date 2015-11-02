#ifndef DASHING_PARSE_NUMBERS_H
#define DASHING_PARSE_NUMBERS_H
#include <vector>
#include <string>
#include <sstream>
#include <boost/algorithm/string/replace.hpp>
namespace dashing
{
std::vector<double> parse_numbers(std::string line);
}
#endif
