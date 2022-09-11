#include "application.hxx"
#include "utils/filesystem.hxx"
#include "utils/input.hxx"
#include "version.hxx" // This file is autogenerated by CMake

#include <libconfig.h++>
#include <iostream>
#include <sys/wait.h>
#include <csignal>
#include <chrono>
#include <ctype.h>

#include <boost/algorithm/string.hpp> // For string lowercase

using namespace StormByte::VideoConvert;

const std::list<std::string> Application::SUPPORTED_MULTIMEDIA_EXTENSIONS	= {
	".asf", ".asx", ".avi", ".wav", ".wma", ".wax", ".wm", ".wmv", ".wvx",
	".ra", ".ram", ".rm", ".rmm",
	".m3u", ".mp2v", ".mpg", ".mpeg", ".m1v", ".mp2", ".mp3", ".mpa",
	".vob",
	"-aif", ".aifc", "-aiff",
	".au", ".snd",
	".ivf",
	".mov", ".qt",
	".flv",
	".mkv", ".mp4" 
};

const std::list<Database::Data::stream_codec> Application::SUPPORTED_CODECS = {
	#ifdef ENABLE_HEVC
	Database::Data::VIDEO_HEVC,
	#endif
	#ifdef ENABLE_AAC
	Database::Data::AUDIO_AAC,
	#endif
	#ifdef ENABLE_FDKAAC
	Database::Data::AUDIO_FDKAAC,
	#endif
	#ifdef ENABLE_AC3
	Database::Data::AUDIO_AC3,
	#endif
	#ifdef ENABLE_EAC3
	Database::Data::AUDIO_EAC3,
	#endif
	#ifdef ENABLE_OPUS
	Database::Data::AUDIO_OPUS,
	#endif
	
	Database::Data::VIDEO_COPY,
	Database::Data::AUDIO_COPY,
	Database::Data::SUBTITLE_COPY
};

Application::Application(): m_status(HALT_ERROR) {
	signal(SIGTERM, Application::signal_handler);
	signal(SIGUSR1, Application::signal_handler); // Reload config and continue working
	signal(SIGUSR2, Application::signal_handler); // Force database scan by awakening process
}

Application& Application::get_instance() {
	static Application instance;
	return instance;
}

int Application::run(int argc, char** argv) noexcept {
	Configuration cli_conf = read_cli(argc, argv); // This updates status flags so needs to be done first
	
	if (m_status != HALT_OK && m_status != HALT_ERROR)
		init(std::move(cli_conf));

	switch(m_status) {
		case RUN_DAEMON:
			return daemon();

		case RUN_INTERACTIVE:
			return interactive();

		case RUN_TEST: // Init was already successful so nothing more to be done
		case HALT_OK:
			return 0;

		case HALT_ERROR:
		default:
			return 1;
	}
}

void Application::init(Configuration&& cli_config) {
	// First we store config file values then we overwrite with CLI values
	m_config = std::move(read_config(cli_config.get_config_file()));
	m_config.merge(std::move(cli_config));
	

	if (!m_config.check(Configuration::OUTPUT_CERR))
		m_status = HALT_ERROR;
	else {
		try {
			m_logger.reset(new Utils::Logger(*m_config.get_log_file(), static_cast<Utils::Logger::LEVEL>(*m_config.get_log_level())));
			m_database.reset(new Database::SQLite3(*m_config.get_database_file()));
		}
		catch(const std::exception& e) {
			header();
			std::cerr << e.what() << std::endl << std::endl;
			help();
			m_status = HALT_ERROR;
		}
	}
}

Configuration Application::read_cli(int argc, char** argv) {
	Configuration config;
	int counter = 1; // Because first item or "argv is always the executable name
	try {
		while (counter < argc) {
			const std::string argument = argv[counter];
			if (argument == "-t" || argument == "--test") {
				m_status = RUN_TEST;
				counter++;
			}
			else if (argument == "-d" || argument == "--daemon") {
				m_status = RUN_DAEMON;
				counter++;
			}
			else if (argument == "-c" || argument == "--config") {
				if (++counter < argc)
					config.set_config_file(argv[counter++]);
				else
					throw std::runtime_error("Config specified without argument, correct usage:");
			}
			else if (argument == "-db" || argument == "--database") {
				if (++counter < argc)
					config.set_database_file(argv[counter++]);
				else
					throw std::runtime_error("Database specified without argument, correct usage:");
			}
			else if (argument == "-i" || argument == "--input") {
				if (++counter < argc)
					config.set_input_folder(argv[counter++]);
				else
					throw std::runtime_error("Input path specified without argument, correct usage:");
			}
			else if (argument == "-o" || argument == "--output") {
				if (++counter < argc)
					config.set_output_folder(argv[counter++]);
				else
					throw std::runtime_error("Output path specified without argument, correct usage:");
			}
			else if (argument == "-w" || argument == "--work") {
				if (++counter < argc)
					config.set_work_folder(argv[counter++]);
				else
					throw std::runtime_error("Work path specified without argument, correct usage:");
			}
			else if (argument == "-l" || argument == "--logfile") {
				if (++counter < argc)
					config.set_log_file(argv[counter++]);
				else
					throw std::runtime_error("Logfile specified without argument, correct usage:");
			}
			else if (argument == "-ll" || argument == "--loglevel") {
				if (++counter < argc) {
					int loglevel;
					if (!Utils::Input::to_int_in_range(argv[counter++], loglevel, 0, Utils::Logger::LEVEL_MAX - 1))
						throw std::runtime_error("Loglevel is not recognized as integer or it has a value not between o and " + std::to_string(Utils::Logger::Logger::LEVEL_MAX - 1));
					config.set_log_level(loglevel);
				}
				else
					throw std::runtime_error("Logfile specified without argument, correct usage:");
			}
			else if (argument == "-s" || argument == "--sleep") {
				if (++counter < argc) {
					int sleep;
					if (Utils::Input::to_int_positive(argv[counter++], sleep) || sleep < 0)
						throw std::runtime_error("Sleep time is not recognized as integer or it has a negative value");
					config.set_sleep_time(sleep);
				}
				else
					throw std::runtime_error("Sleep time specified without argument, correct usage:");
			}
			else if (argument == "-a" || argument == "--add") {
				if (++counter < argc) {
					// We do here a very basic unscape for bash scaped characters
					config.set_interactive_parameter(boost::erase_all_copy(std::string(argv[counter++]), "\\"));
					m_status = RUN_INTERACTIVE;
				}
				else
					throw std::runtime_error("Add film specified without argument, correct usage:");
			}
			else if (argument == "-v" || argument == "--version") {
				version();
				m_status = HALT_OK;
				counter++; // To keep parsing CLI
			}
			else if (argument == "-h" || argument == "--help") {
				header();
				help();
				m_status = HALT_OK;
				counter++; // To keep parsinc CLI
			}
			else
				throw std::runtime_error("Unknown argument: " + argument + ", correct usage:");

		}
		if(m_status == HALT_ERROR) // If no action specified the default is HALT_ERROR
			throw std::runtime_error("No action specified, select --add(-a), --daemon(-d) or --test(-t) to execute the program");
	}
	catch(const std::runtime_error& exception) {
		header();
		std::cerr << exception.what() << std::endl << std::endl;
		help();
		m_status = HALT_ERROR;
	}
	return config;
}

Configuration Application::read_config(const std::filesystem::path& config_file) {
	Configuration config;
	libconfig::Config cfg;
	
	try {
    	cfg.readFile(config_file.c_str());
	}
	catch(const libconfig::FileIOException &fioex) {
		std::cerr << "Can not open configuration file " << config_file << std::endl;
		m_status = HALT_ERROR;
		return config;
	}
	catch(const libconfig::ParseException &pex) {
		std::cerr << "Parse error reading configuration file " << config_file << " at line " << std::to_string(pex.getLine()) << std::endl;
		m_status = HALT_ERROR;
		return config;
	}

	if (cfg.exists("database"))
		config.set_database_file(std::string(cfg.lookup("database")));
	if (cfg.exists("input"))
		config.set_input_folder(std::string(cfg.lookup("input")));
	if (cfg.exists("output"))
		config.set_output_folder(std::string(cfg.lookup("output")));
	if (cfg.exists("work"))
		config.set_work_folder(std::string(cfg.lookup("work")));
	if (cfg.exists("logfile"))
		config.set_log_file(std::string(cfg.lookup("logfile")));
	if (cfg.exists("loglevel") && cfg.lookup("loglevel").isNumber())
		config.set_log_level(static_cast<int>(cfg.lookup("loglevel")));
	if (cfg.exists("sleep") && cfg.lookup("sleep").isNumber())
		config.set_sleep_time(static_cast<int>(cfg.lookup("sleep")));

	return config;
}

void Application::header() const {
	const std::string caption = PROGRAM_NAME + " " + PROGRAM_VERSION + " by " + PROGRAM_AUTHOR;
	std::cout << caption << std::endl;
	std::cout << std::string(caption.size(), '=') << std::endl;
	std::cout << PROGRAM_DESCRIPTION << std::endl << std::endl;
}

void Application::help() const {
	std::cout << "This is the list of available options:" << std::endl;
	std::cout << "\t-t,  --test\t\tTest if program can be run without any other action" << std::endl;
	std::cout << "\t-d,  --daemon\t\tRun daemon reading database items to keep converting files" << std::endl;
	std::cout << "\t-c,  --config <file>\tSpecifies a config file instead of the default " << Configuration::DEFAULT_CONFIG_FILE << std::endl;
	std::cout << "\t-a,  --add <file>\tInteractivelly add a new film to database files" << std::endl;
	std::cout << "\t-db, --database <file>\tSpecify SQLite database file to be used" << std::endl;
	std::cout << "\t-i,  --input <folder>\tSpecify input folder to read films from" << std::endl;
	std::cout << "\t-o,  --output <folder>\tSpecify output folder to store converted files once finished" << std::endl;
	std::cout << "\t-w,  --work <folder>\tSpecify temprary working folder to store files while being converted" << std::endl;
	std::cout << "\t-l,  --logfile <file>\tSpecify a file for storing logs" << std::endl;
	std::cout << "\t-ll, --loglevel <level>\tSpecify which loglevel to display (Should be between 0 and " << std::to_string(Utils::Logger::Logger::LEVEL_MAX - 1) << ")" << std::endl; 
	std::cout << "\t-s,  --sleep <seconds>\tSpecify the time to sleep in main loop. Of course should be positive integer unless you are my boyfriend and have that ability ;)" << std::endl;
	std::cout << "\t-v,  --version\t\tShow version and compile information" << std::endl;
	std::cout << "\t-h,  --help\t\tShow this message" << std::endl;
	std::cout << std::endl;
	std::cout << "Please note that every unrecognized option in config file will be ignored but every unrecognized option in command line will throw an error." << std::endl;
}

void Application::version() const {
	std::cout << PROGRAM_NAME + " " + PROGRAM_VERSION + " by " + PROGRAM_AUTHOR << std::endl;
	compiler_info();
}

void Application::compiler_info() const {
	std::cout << "Compiled by " << COMPILER_NAME << "(" << COMPILER_VERSION << ")" << " with flags " << COMPILER_FLAGS << std::endl;
}

std::string Application::elapsed_time(const std::chrono::steady_clock::time_point& begin, const std::chrono::steady_clock::time_point& end) const {
	std::string result = "";
	auto hours = std::chrono::duration_cast<std::chrono::hours>(end - begin).count();
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(end - begin).count();
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(end - begin).count();

	result += std::to_string(hours);
	result += ":";
	if ((minutes % 60) < 10) result += "0";
	result += std::to_string(minutes % 60);
	result += ":";
	if ((seconds % 3600) < 10) result += "0";
	result += std::to_string(seconds % 3600);

	return result;
}

int Application::daemon() {
	m_logger->message_line(Utils::Logger::LEVEL_INFO, "Starting daemon version " + PROGRAM_VERSION);
	m_logger->message_line(Utils::Logger::LEVEL_DEBUG, "Resetting previously in process films");
	m_database->reset_processing_films();
	do {
		m_logger->message_line(Utils::Logger::LEVEL_NOTICE, "Checking for films to convert...");
		auto film = m_database->get_film_for_process();
		if (film) {
			m_logger->message_line(Utils::Logger::LEVEL_INFO, "Film " + film->get_input_file().string() + " found");
			execute_ffmpeg(*film);
		}
		else {
			m_logger->message_line(Utils::Logger::LEVEL_NOTICE, "No films found");
			m_logger->message_line(Utils::Logger::LEVEL_NOTICE, "Sleeping " + std::to_string(m_config.get_sleep_time()) + " seconds before retrying");
			sleep(m_config.get_sleep_time());
		}
	} while(m_status != HALT_OK && m_status != HALT_ERROR);
	m_logger->message_line(Utils::Logger::LEVEL_INFO, "Stopping daemon...");

	return (m_status == HALT_OK) ? 0 : 1;
}

void Application::execute_ffmpeg(const FFmpeg& ffmpeg) {
	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	std::filesystem::path input_file = ffmpeg.get_full_input_file();
	std::filesystem::path work_file = ffmpeg.get_full_work_file();
	std::filesystem::path output_path = ffmpeg.get_full_output_path();
	std::filesystem::path output_file = ffmpeg.get_full_output_file();

	m_logger->message_line(Utils::Logger::LEVEL_DEBUG, "Marking film " + input_file.string() + " as being processed in database");
	m_database->set_film_processing_status(ffmpeg.get_film_id(), true);
	m_worker = ffmpeg.exec();
	int status;
	wait(&status);
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	m_worker.reset(); // Worker has finished
	if (status == 0) {
		m_logger->message_line(Utils::Logger::LEVEL_INFO, "Conversion for " + input_file.string() + " finished in " + elapsed_time(begin, end));
		if (!std::filesystem::exists(output_path)) {
			m_logger->message_line(Utils::Logger::LEVEL_NOTICE, "Creating output path " + output_path.string());
			std::filesystem::create_directories(output_path);
		}
		m_logger->message_line(Utils::Logger::LEVEL_INFO, "Copying file " + work_file.string() + " to " + output_file.string());
		std::filesystem::copy_file(work_file, output_file);
		m_logger->message_line(Utils::Logger::LEVEL_INFO, "Deleting original input file " + input_file.string());
		std::filesystem::remove(input_file);
		m_logger->message_line(Utils::Logger::LEVEL_INFO, "Deleting original work file " + work_file.string());
		std::filesystem::remove(work_file);
		m_logger->message_line(Utils::Logger::LEVEL_DEBUG, "Deleting film from database");
		m_database->delete_film(ffmpeg.get_film_id());
		if (ffmpeg.get_group() && m_database->is_group_empty(ffmpeg.get_group()->id)) {
			m_logger->message_line(Utils::Logger::LEVEL_INFO, "Deleting folder " + (*m_config.get_input_folder() / ffmpeg.get_group()->folder).string() + " recursivelly");
			std::filesystem::remove_all(*m_config.get_input_folder() / ffmpeg.get_group()->folder);
			m_logger->message_line(Utils::Logger::LEVEL_INFO, "Deleting folder " + (*m_config.get_work_folder() / ffmpeg.get_group()->folder).string() + " recursivelly");
			std::filesystem::remove_all(*m_config.get_work_folder() / ffmpeg.get_group()->folder);
			m_logger->message_line(Utils::Logger::LEVEL_DEBUG, "Deleting group from database");
			m_database->delete_group(ffmpeg.get_group()->id);
		}
	}
	else {
		m_logger->message_line(Utils::Logger::LEVEL_ERROR, "Conversion for " + input_file.string() + " failed or interrupted!");
		m_logger->message_line(Utils::Logger::LEVEL_INFO, "Deleting temporary unfinished file " + work_file.string());
		std::filesystem::remove(work_file);
		m_logger->message_line(Utils::Logger::LEVEL_DEBUG, "Marking film " + input_file.string() + " as unsupported in database");
		m_database->set_film_unsupported_status(ffmpeg.get_film_id(), false);
	}
}

int Application::interactive() {	
	header();
	const std::filesystem::path full_path = *m_config.get_input_folder() / *m_config.get_interactive_parameter();

	if (!std::filesystem::exists(full_path)) {
		std::cerr << "File " << (std::filesystem::is_directory(full_path) ? "path " : "") << full_path << " does not exist" << std::endl;
		return 1;
	}
	else if (m_database->is_film_in_database(*m_config.get_interactive_parameter())) {
		std::cerr << "Film " << *m_config.get_interactive_parameter() << " is already in database!" << std::endl;
		return 1;
	}
	else if (std::filesystem::is_directory(full_path) && m_database->is_group_in_database(*m_config.get_interactive_parameter())) {
		std::cerr << "Film group (folder) " << *m_config.get_interactive_parameter() << " is already in database" << std::endl;
		return 1;
	}
	
	/* Query required data */
	auto films = std::move(ask_film_data());
	if (!films) return 1;
	auto streams = std::move(ask_streams());

	if (add_films_to_database(std::move(*films), std::move(streams)))
		return 0;
	else
		return 1;
}

std::optional<std::list<Database::Data::film>> Application::ask_film_data() const {
	std::string buffer_str;
	int buffer_int;
	std::list<Database::Data::film> films;
	std::list<std::filesystem::path> unsupported_films;
	Database::Data::film film;
	do {
		std::cout << "Which priority (default NORMAL)? LOW(0), NORMAL(1), HIGH(2), IMPORTANT(3): ";
		std::getline(std::cin, buffer_str);
	} while (buffer_str != "" && !Utils::Input::to_int_in_range(buffer_str, buffer_int, 0, 3, true));
	film.prio = (buffer_str == "") ? 1 : buffer_int;

	// Now we look if a single film was specified or if it was a folder
	const std::filesystem::path full_path = *m_config.get_input_folder() / *m_config.get_interactive_parameter();
	const std::filesystem::path input_folder = *m_config.get_input_folder();
	if (std::filesystem::is_directory(full_path)) {
		film.group = m_database->insert_group(*m_config.get_interactive_parameter());

		// Look for all supported films in directory
		for (std::filesystem::recursive_directory_iterator i(full_path), end; i != end; ++i) 
			if (!std::filesystem::is_directory(i->path())) {
				std::string extension = boost::to_lower_copy(i->path().extension().string());
				if (std::find(SUPPORTED_MULTIMEDIA_EXTENSIONS.begin(), SUPPORTED_MULTIMEDIA_EXTENSIONS.end(), extension) != SUPPORTED_MULTIMEDIA_EXTENSIONS.end()) {
					film.file = std::filesystem::relative(i->path(), input_folder);
					films.push_back(film);
				}
				else {
					unsupported_films.push_back(std::filesystem::relative(i->path(), input_folder));
				}
			}
	}
	else {
		// Single file
		film.file = *m_config.get_interactive_parameter();
		films.push_back(film);
	}

	if (!unsupported_films.empty()) {
		std::cout << "Found " << std::to_string(unsupported_films.size()) << " unsupported films:\n";
		for (auto it = unsupported_films.begin(); it != unsupported_films.end(); it++)
			std::cout << "\t* " << (*it) << "\n";
		std::cout << std::endl;
		do {
			std::cout << "Do you wish to continue? [y/n]: ";
			std::getline(std::cin, buffer_str);
		} while(!Utils::Input::in_options(buffer_str, { "y", "Y", "n", "N" }));
		if (buffer_str == "n" || buffer_str == "N") return std::optional<std::list<Database::Data::film>>();
	}

	return films;
}

std::list<Database::Data::stream> Application::ask_streams() {
	std::map<char, bool> continue_asking {
		{ 'v', true },
		{ 'a', true },
		{ 's', true }
	};
	std::list<Database::Data::stream> streams;
	bool add_new_stream, continue_asking_any;

	do {
		std::string buffer_str;		
		do {
			std::cout << "Select stream type; video(v), audio(a) or subtitles(s): ";
			std::getline(std::cin, buffer_str);
		} while (!Utils::Input::in_options(buffer_str, { "v", "V", "a", "A", "s", "S" }, true));
		char codec_type = tolower(buffer_str[0]);

		if (!continue_asking[codec_type]) {
			std::cerr << "Selected stream type " << codec_type << " has been already fully defined" << std::endl;
		}
		else {
			auto stream = std::move(ask_stream(codec_type));
			continue_asking[codec_type] = stream.id >= 0; // If negative, means all streams have been covered
			streams.push_back(std::move(stream));
			continue_asking_any = continue_asking['v'] || continue_asking['a'] || continue_asking['s'];

			if (continue_asking_any)
				do {
					buffer_str = "";
					std::cout << "Add another stream? [y/n]: ";
					std::getline(std::cin, buffer_str);
				} while(!Utils::Input::in_options(buffer_str, { "y", "Y", "n", "N" }));
			else
				std::cout << "There are no more streams to add" << std::endl;

			add_new_stream = buffer_str == "y" || buffer_str == "Y";
		}
	} while(add_new_stream && continue_asking_any);

	return streams;
}

Database::Data::stream Application::ask_stream(const char& codec_type) const {
	Database::Data::stream stream;
	std::string buffer_str;
	int buffer_int;

	do {
		std::cout << "Input stream(" << codec_type << ") FFmpeg ID (-1 to select all streams for type " << codec_type << "): ";
		std::getline(std::cin, buffer_str);
	} while(!Utils::Input::to_int_minimum(buffer_str, buffer_int, -1, true));
	stream.id = buffer_int;

	if (codec_type == 'v') {
		do {
			std::cout << "Select video codec:" << std::endl;
			std::cout << "\tcopy(" << Database::Data::VIDEO_COPY << ")" << std::endl;
			#ifdef ENABLE_HEVC
			std::cout << "\tHEVC(" << Database::Data::VIDEO_HEVC << ")" << std::endl;
			#endif
			std::cout << "Which codec to use?: ";
			std::getline(std::cin, buffer_str);
		} while (!Utils::Input::to_int(buffer_str, buffer_int, true) || !Utils::Input::in_options(
			buffer_str,
			{
				#ifdef ENABLE_HEVC
				std::to_string(Database::Data::VIDEO_HEVC),
				#endif
				std::to_string(Database::Data::VIDEO_COPY)
			},
			true
		));
		stream.codec = static_cast<Database::Data::stream_codec>(buffer_int);

		// Only certain codecs supports animation, TODO: Make it better
		if (stream.codec == Database::Data::VIDEO_HEVC) {
			do {
				buffer_str = "";
				std::cout << "Is an animated movie? [y/n]: ";
				std::getline(std::cin, buffer_str);
			} while (!Utils::Input::in_options(buffer_str, { "y", "Y", "n", "N" }));
			if (buffer_str == "y" || buffer_str == "Y")
				stream.is_animation = true;
		}
	
		#ifdef ENABLE_HEVC
		if (stream.codec == Database::Data::VIDEO_HEVC) {
			do {
				std::cout << "Does it have HDR? [y/n]: ";
				std::getline(std::cin, buffer_str);
			} while(!Utils::Input::in_options(
				buffer_str,
				{ "y", "Y", "n", "N" }
			));
			if (buffer_str == "y" || buffer_str == "Y")
				stream.HDR = ask_stream_hdr();
		}
		#endif
	}
	else if (codec_type == 'a') {
		do {
			std::cout << "Select audio codec:" << std::endl;
			#ifdef ENABLE_AAC
			std::cout << "\tAAC(" << Database::Data::AUDIO_AAC << ")" << std::endl;
			#endif
			#ifdef ENABLE_FDKAAC
			std::cout << "\tFDKAAC(" << Database::Data::AUDIO_FDKAAC << ")" << std::endl;
			#endif
			#ifdef ENABLE_AC3
			std::cout << "\tAC-3(" << Database::Data::AUDIO_AC3 << ")" << std::endl;
			#endif
			#ifdef ENABLE_EAC3
			std::cout << "\tE-AC3(" << Database::Data::AUDIO_EAC3 << ")" << std::endl;
			#endif
			#ifdef ENABLE_OPUS
			std::cout << "\tOpus(" << Database::Data::AUDIO_OPUS << ")" << std::endl;
			#endif
			std::cout << "\tcopy(" << Database::Data::AUDIO_COPY << ")" << std::endl;
			std::cout << "Which codec to use?: ";
			std::getline(std::cin, buffer_str);
		} while (!Utils::Input::to_int(buffer_str, buffer_int, true) || !Utils::Input::in_options(
			buffer_str,
			{
				#ifdef ENABLE_AAC
				std::to_string(Database::Data::AUDIO_AAC),
				#endif
				#ifdef ENABLE_AC3
				std::to_string(Database::Data::AUDIO_AC3),
				#endif
				#ifdef ENABLE_EAC3
				std::to_string(Database::Data::AUDIO_EAC3),
				#endif
				#ifdef ENABLE_OPUS
				std::to_string(Database::Data::AUDIO_OPUS),
				#endif
				#ifdef ENABLE_FDKAAC
				std::to_string(Database::Data::AUDIO_FDKAAC),
				#endif
				std::to_string(Database::Data::AUDIO_COPY),
			},
			true
		));
		stream.codec = static_cast<Database::Data::stream_codec>(buffer_int);
	}
	else {
		std::cout << "Subtitles have only copy codec so it is being autoselected" << std::endl;
		stream.codec = Database::Data::SUBTITLE_COPY;
	}

	return stream;
}

bool Application::add_films_to_database(const std::list<Database::Data::film>& films, std::list<Database::Data::stream>&& streams) {
	try {
		m_database->begin_transaction();
		std::optional<int> filmID;
		for (auto film = films.begin(); film != films.end(); film++) {
			if (m_database->is_film_in_database(*film))
				throw std::runtime_error("ERROR: Film " + film->file.string() + " is already in database");
			else
				filmID = m_database->insert_film(*film);

			if (!filmID)
				throw std::runtime_error("ERROR: Film " + film->file.string() + " could not be inserted in database");

			for (auto stream = streams.begin(); stream != streams.end(); stream++) {
				// First is to assign film ID to stream
				stream->film_id = *filmID;
				m_database->insert_stream(*stream);
			}
		}
	}
	catch(const std::runtime_error& error) {
		m_database->rollback_transaction();
		std::cerr << error.what() << std::endl;
		return false;
	}
	m_database->commit_transaction();
	std::cout << "Inserted " << std::to_string(films.size()) + " film(s) in database" << std::endl;
	return true;
}

#ifdef ENABLE_HEVC
Database::Data::hdr Application::ask_stream_hdr() const {
	Database::Data::hdr HDR;
	std::string buffer_str;
	int buffer_int;

	std::cout << "Input HDR parameters (leave empty to use default value):" << std::endl;

	do {
		buffer_str = "";
		std::cout << "red x (" << Stream::Video::HEVC::HDR::DEFAULT_REDX << "): ";
		std::getline(std::cin, buffer_str);
	} while(buffer_str != "" && !Utils::Input::to_int_positive(buffer_str, buffer_int, true));
	HDR.red_x = (buffer_str == "") ? Stream::Video::HEVC::HDR::DEFAULT_REDX : buffer_int;

	do {
		buffer_str = "";
		std::cout << "red y (" << Stream::Video::HEVC::HDR::DEFAULT_REDY << "): ";
		std::getline(std::cin, buffer_str);
	} while(buffer_str != "" && !Utils::Input::to_int_positive(buffer_str, buffer_int, true));
	HDR.red_y = (buffer_str == "") ? Stream::Video::HEVC::HDR::DEFAULT_REDY : buffer_int;

	do {
		buffer_str = "";
		std::cout << "green x (" << Stream::Video::HEVC::HDR::DEFAULT_GREENX << "): ";
		std::getline(std::cin, buffer_str);
	} while(buffer_str != "" && !Utils::Input::to_int_positive(buffer_str, buffer_int, true));
	HDR.green_x = (buffer_str == "") ? Stream::Video::HEVC::HDR::DEFAULT_GREENX : buffer_int;

	do {
		buffer_str = "";
		std::cout << "green y (" << Stream::Video::HEVC::HDR::DEFAULT_GREENY << "): ";
		std::getline(std::cin, buffer_str);
	} while(buffer_str != "" && !Utils::Input::to_int_positive(buffer_str, buffer_int, true));
	HDR.green_y = (buffer_str == "") ? Stream::Video::HEVC::HDR::DEFAULT_GREENY : buffer_int;

	do {
		buffer_str = "";
		std::cout << "blue x (" << Stream::Video::HEVC::HDR::DEFAULT_BLUEX << "): ";
		std::getline(std::cin, buffer_str);
	} while(buffer_str != "" && !Utils::Input::to_int_positive(buffer_str, buffer_int, true));
	HDR.blue_x = (buffer_str == "") ? Stream::Video::HEVC::HDR::DEFAULT_BLUEX : buffer_int;

	do {
		buffer_str = "";
		std::cout << "blue y (" << Stream::Video::HEVC::HDR::DEFAULT_BLUEY << "): ";
		std::getline(std::cin, buffer_str);
	} while(buffer_str != "" && !Utils::Input::to_int_positive(buffer_str, buffer_int, true));
	HDR.blue_y = (buffer_str == "") ? Stream::Video::HEVC::HDR::DEFAULT_BLUEY : buffer_int;

	do {
		buffer_str = "";
		std::cout << "white point x (" << Stream::Video::HEVC::HDR::DEFAULT_WHITEPOINTX << "): ";
		std::getline(std::cin, buffer_str);
	} while(buffer_str != "" && !Utils::Input::to_int_positive(buffer_str, buffer_int, true));
	HDR.white_point_x = (buffer_str == "") ? Stream::Video::HEVC::HDR::DEFAULT_WHITEPOINTX : buffer_int;

	do {
		buffer_str = "";
		std::cout << "white point y (" << Stream::Video::HEVC::HDR::DEFAULT_WHITEPOINTY << "): ";
		std::getline(std::cin, buffer_str);
	} while(buffer_str != "" && !Utils::Input::to_int_positive(buffer_str, buffer_int, true));
	HDR.white_point_y = (buffer_str == "") ? Stream::Video::HEVC::HDR::DEFAULT_WHITEPOINTY : buffer_int;

	do {
		buffer_str = "";
		std::cout << "luminance min (" << Stream::Video::HEVC::HDR::DEFAULT_LUMINANCEMIN << "): ";
		std::getline(std::cin, buffer_str);
	} while(buffer_str != "" && !Utils::Input::to_int_positive(buffer_str, buffer_int, true));
	HDR.luminance_min = (buffer_str == "") ? Stream::Video::HEVC::HDR::DEFAULT_LUMINANCEMIN : buffer_int;

	do {
		buffer_str = "";
		std::cout << "luminance max (" << Stream::Video::HEVC::HDR::DEFAULT_LUMINANCEMAX << "): ";
		std::getline(std::cin, buffer_str);
	} while(buffer_str != "" && !Utils::Input::to_int_positive(buffer_str, buffer_int, true));
	HDR.luminance_max = (buffer_str == "") ? Stream::Video::HEVC::HDR::DEFAULT_LUMINANCEMAX : buffer_int;

	do {
		buffer_str = "";
		std::cout << "Does it have light level data? [y/n]: ";
		std::getline(std::cin, buffer_str);
	} while(!Utils::Input::in_options(buffer_str, { "y", "Y", "n", "N" }, true));

	if (buffer_str == "y" || buffer_str == "Y") {
		do {
			buffer_str = "";
			std::cout << "light level max: ";
			std::getline(std::cin, buffer_str);
		} while(!Utils::Input::to_int_positive(buffer_str, buffer_int, true));
		HDR.light_level_max = buffer_int;

		do {
			buffer_str = "";
			std::cout << "light level average: ";
			std::getline(std::cin, buffer_str);
		} while(!Utils::Input::to_int_positive(buffer_str, buffer_int, true));
		HDR.light_level_average = buffer_int;
	}

	return HDR;
}
#endif

void Application::signal_handler(int signal) {
	auto& app_instance = Application::get_instance();
	app_instance.m_logger->message_line(Utils::Logger::LEVEL_NOTICE, "Signal " + std::to_string(signal) + " received!");

	switch(signal) {
		case SIGTERM:
			app_instance.m_status = HALT_OK;
			if (app_instance.m_worker)
				kill(*app_instance.m_worker, SIGTERM);
			break;

		case SIGUSR1: {
			app_instance.m_logger->message_line(Utils::Logger::LEVEL_INFO, "Reloading config");

			std::list<std::string> errors;

			// Only loglevel and sleep time are allowed to change while running so we need to check if we have to stop otherwise
			auto old_db_value 		= app_instance.m_config.get_database_file();
			auto old_input_value 	= app_instance.m_config.get_input_folder();
			auto old_output_value 	= app_instance.m_config.get_output_folder();
			auto old_work_value 	= app_instance.m_config.get_work_folder();
			auto old_logfile_value 	= app_instance.m_config.get_log_file();

			app_instance.m_config.merge(app_instance.read_config(app_instance.m_config.get_config_file()));

			if (old_db_value != app_instance.m_config.get_database_file())
				errors.push_back("Database file");

			if (old_input_value != app_instance.m_config.get_input_folder())
				errors.push_back("Input folder");

			if (old_output_value != app_instance.m_config.get_output_folder())
				errors.push_back("Output folder");

			if (old_work_value != app_instance.m_config.get_work_folder())
				errors.push_back("Work folder");

			if (old_logfile_value != app_instance.m_config.get_log_file())
				errors.push_back("Log file");

			if (!errors.empty()) {
				errors.push_front("Config reloading error: The following " + std::to_string(errors.size()) + " items have changed which are not permitted while daemon is running!");
				for (auto it = errors.begin(); it != errors.end(); it++) {
					app_instance.m_logger->message_line(Utils::Logger::LEVEL_FATAL, *it);
				}
				app_instance.m_status = HALT_ERROR;
			}
			break;
		}

		case SIGUSR2:
		default:
			// No action as this will only wake up from sleep
			break;
	}
}
