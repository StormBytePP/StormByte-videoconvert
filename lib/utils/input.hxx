#pragma once

#include <string>
#include <list>

namespace StormByte::VideoConvert::Utils {
	class Input {
		public:
			static bool is_int(const std::string& possible_int, const bool& use_cerr = false);
			static bool to_int(const std::string& possible_number, int& store, const bool& use_cerr = false);
			static bool to_int_positive(const std::string& possible_number, int& store, const bool& use_cerr = false);
			static bool to_int_minimum(const std::string& possible_number, int& store, const int& minimum, const bool& use_cerr = false);
			static bool to_int_in_range(const std::string& possible_number, int& store, const int& min, const int& max, const bool& use_cerr = false);
			static bool in_range(const int& number, const int& min, const int& max, const bool& use_cerr = false);
			static bool in_options(const std::string& input, const std::list<std::string>& options, const bool& use_cerr = false);
			static bool in_options(const int& input, const std::list<int>& options, const bool& use_cerr = false);
			static bool in_yes_no(const std::string input, bool& store, const bool& use_cerr = false);
	};
}
