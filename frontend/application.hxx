#pragma once

#include "database/sqlite3.hxx"
#include "utils/logger.hxx"

#include <filesystem>
#include <string>

namespace StormByte::VideoConvert {
	class Application {
		public:
			Application(const Application&) = delete;
			Application& operator=(const Application&) = delete;
			~Application() = default;

			static Application& get_instance();
			int run(int argc, char** argv) noexcept;

			inline std::filesystem::path get_input_folder() const { return m_input_path.value_or(""); }
			inline std::filesystem::path get_output_folder() const { return m_output_path.value_or(""); }
			inline std::filesystem::path get_work_folder() const { return m_work_path.value_or(""); }
			inline std::shared_ptr<Utils::Logger> get_logger() const { return m_logger; }

			static const std::string PROGRAM_NAME, PROGRAM_DESCRIPTION, PROGRAM_AUTHOR, PROGRAM_VERSION;
			static const std::string COMPILER_NAME, COMPILER_VERSION, COMPILER_FLAGS;
			static const std::string FFMPEG_EXECUTABLE;

			static const std::list<Database::Data::stream_codec> SUPPORTED_CODECS;

		private:
			/* Config data */
			std::optional<std::filesystem::path> m_database_file, m_input_path, m_output_path, m_work_path, m_logfile;
			std::optional<short> m_loglevel; // To not confuse users if they set it in negative in config
			int m_sleep_idle_seconds;

			bool m_daemon_mode, m_pretend_run;
			std::unique_ptr<StormByte::VideoConvert::Database::SQLite3> m_database;
			std::shared_ptr<StormByte::VideoConvert::Utils::Logger> m_logger;
			std::optional<pid_t> m_worker;
			volatile bool m_must_terminate;
			std::optional<std::filesystem::path> m_add_film_path;

			static const std::filesystem::path DEFAULT_CONFIG_FILE;
			static const unsigned int DEFAULT_SLEEP_IDLE_SECONDS;
			static const std::list<std::string> SUPPORTED_MULTIMEDIA_EXTENSIONS; 
			static void signal_handler(int);
			enum status { CONTINUE = 0, HALT_OK, ERROR };

			Application(); // Hide constructor
			bool init_from_config();
			status init_from_cli(int argc, char** argv);
			bool init_application();
			void header() const;
			void help() const;
			void version() const;
			void compiler_info() const;
			std::string elapsed_time(const std::chrono::steady_clock::time_point& begin, const std::chrono::steady_clock::time_point& end) const;

			int daemon();
			void execute_ffmpeg(const FFmpeg& ffmpeg);
			int interactive(const std::filesystem::path& film_file_or_path);
			std::list<Database::Data::film> ask_film_data(const std::filesystem::path& file_or_path) const;
			std::list<Database::Data::stream> ask_streams();
			Database::Data::stream ask_stream(const char& codec_type) const;
			bool add_films_to_database(const std::list<Database::Data::film>& films, std::list<Database::Data::stream>&& streams);
			#ifdef ENABLE_HEVC // HDR is only available if HEVC/H265 is supported
			Database::Data::hdr ask_stream_hdr() const;
			#endif
	};
}
