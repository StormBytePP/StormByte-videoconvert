#pragma once

#include "types.hxx"

#include <fstream>

namespace StormByte::VideoConvert::Utils {
	class Logger {
		public:
			enum LEVEL:unsigned short {
				LEVEL_DEBUG = 0,
				LEVEL_WARNING,
				LEVEL_NOTICE,
				LEVEL_INFO,
				LEVEL_ERROR,
				LEVEL_FATAL,
				LEVEL_MAX
			};
			Logger(const Types::path_t& logfile, LEVEL display_level);
			Logger(const Logger&) = delete;
			Logger(Logger&&) = delete;
			Logger& operator=(const Logger&) = delete;
			Logger& operator=(Logger&&) = delete;
			~Logger();
			
			void message_part_begin(const LEVEL& level, const std::string& msg);
			void message_part_continue(const LEVEL& level, const std::string& msg);
			void message_part_end(const LEVEL& level, const std::string& msg);
			void message_line(const LEVEL& level, const std::string& msg);
			void end_line(const LEVEL& level);
			

		private:
			std::ofstream m_logfile;
			LEVEL m_display_level;

			void header(const LEVEL& log_level);
			void timestamp();
			void loglevel_display(const LEVEL& level);
	};
}
