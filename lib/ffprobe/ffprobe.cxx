#include "ffprobe.hxx"
#include "utils/input.hxx"
#include "task/execute/ffprobe/stream.hxx"
#include "task/execute/ffprobe/video_color.hxx"
#include "task/execute/ffprobe/video_resolution.hxx"

using namespace StormByte::VideoConvert;

const std::map<FFprobe::stream::RESOLUTION, std::string> FFprobe::stream::RESOLUTION_STRING = {
	{ RES_480P,		"480p" },
	{ RES_720P,		"720p" },
	{ RES_1080P,	"1080p" },
	{ RES_4K,		"4K" },
	{ RES_8K,		"8K" }
};

const std::map<FFprobe::stream::RESOLUTION, unsigned short> FFprobe::stream::RESOLUTION_MAX_HEIGHT = {
	{ RES_480P,		480 },
	{ RES_720P,		720 },
	{ RES_1080P,	1080 },
	{ RES_4K,		2160 },
	{ RES_8K,		4320 }
};

FFprobe::FFprobe() {
	m_streams = {
		{ stream::VIDEO,	std::vector<stream>() },
		{ stream::AUDIO,	std::vector<stream>() },
		{ stream::SUBTITLE,	std::vector<stream>() }
	};
}

FFprobe FFprobe::from_file(const Types::path_t& file) noexcept {
	FFprobe probe;
	probe.initialize(file);
	return probe;
}

void FFprobe::initialize(const Types::path_t& file) noexcept {
	std::unique_ptr<Task::Execute::FFprobe::Base> task;

	task.reset(new Task::Execute::FFprobe::VideoColor(file));
	if (task->run() == Task::HALT_OK)
		initialize_video_color_data(task->get_stdout());
	for (const auto type : { stream::VIDEO, stream::AUDIO, stream::SUBTITLE }) {
		task.reset(new Task::Execute::FFprobe::Stream(file, type));
		if (task->run() == Task::HALT_OK)
			initialize_stream_data(task->get_stdout(), type);
	}
	task.reset(new Task::Execute::FFprobe::VideoResolution(file));
	if (task->run() == Task::HALT_OK)
		initialize_video_resolution(task->get_stdout());
}

void FFprobe::initialize_video_color_data(const std::string& json) {
	std::optional<Json::Value> root = parse_json(json);
	if (root) {
		auto json = *(root->begin());
		Json::Value item;

		try {
			for (auto it = json[0].begin(); it != json[0].end(); it++) {
				if (it.key() == "pix_fmt" && !it->isNull()) m_pix_fmt						= it->asString();
				else if (it.key() == "color_space" && !it->isNull()) m_color_space			= it->asString();
				else if (it.key() == "color_primaries" && !it->isNull()) m_color_primaries	= it->asString();
				else if (it.key() == "color_transfer" && !it->isNull()) m_color_transfer	= it->asString();
				#ifdef ENABLE_HEVC
				else if (it.key() == "side_data_list" && !it->isNull()) {
					// Here it is the HDR color data
					// It will have 2 sections: color and luminance (optional)
					for (Json::Value::ArrayIndex i = 0; i < it->size(); i++) {
						for (auto it2 = (*it)[i].begin(); it2 != (*it)[i].end(); it2++) {
							if (it2.key() == "red_x" && !it2->isNull()) m_red_x 						= it2->asString().substr(0, it2->asString().find("/"));
							else if (it2.key() == "red_y" && !it2->isNull()) m_red_y 					= it2->asString().substr(0, it2->asString().find("/"));
							else if (it2.key() == "green_x" && !it2->isNull()) m_green_x				= it2->asString().substr(0, it2->asString().find("/"));
							else if (it2.key() == "green_y" && !it2->isNull()) m_green_y				= it2->asString().substr(0, it2->asString().find("/"));
							else if (it2.key() == "blue_x" && !it2->isNull()) m_blue_x 					= it2->asString().substr(0, it2->asString().find("/"));
							else if (it2.key() == "blue_y" && !it2->isNull()) m_blue_y 					= it2->asString().substr(0, it2->asString().find("/"));
							else if (it2.key() == "white_point_x" && !it2->isNull()) m_white_point_x	= it2->asString().substr(0, it2->asString().find("/"));
							else if (it2.key() == "white_point_y" && !it2->isNull()) m_white_point_y	= it2->asString().substr(0, it2->asString().find("/"));
							else if (it2.key() == "min_luminance" && !it2->isNull()) m_min_luminance	= it2->asString().substr(0, it2->asString().find("/"));
							else if (it2.key() == "max_luminance" && !it2->isNull()) m_max_luminance	= it2->asString().substr(0, it2->asString().find("/"));
							else if (it2.key() == "max_average" && !it2->isNull()) m_max_average		= it2->asString();
							else if (it2.key() == "max_content" && !it2->isNull()) m_max_content		= it2->asString();
						}
					}
				}
				#endif
			}
		}
		catch (const std::exception& e) {
			//Something went wrong but we ignore it
		}
	}
}

void FFprobe::initialize_video_resolution(const std::string& json) {
	std::optional<Json::Value> root = parse_json(json);
	if (root) {
		auto json = (*root)["streams"];
		
		try {
			for (auto it = json.begin(); it != json.end(); it++)
				for (auto it2 = it->begin(); it2 != it->end(); it2++)
					if (it2.key() == "height" && it2->isUInt()) m_height = it2->asUInt();
					else if (it2.key() == "width" && it2->isUInt()) m_width = it2->asUInt();
		}
		catch(const std::exception& e) {
			// We just ignore
		}
	}
}

void FFprobe::initialize_stream_data(const std::string& json, const stream::TYPE& type) {
	m_streams[type].clear();
	std::optional<Json::Value> root = parse_json(json);
	if (root) {
		auto json = (*root)["streams"];

		try {
			for (Json::ArrayIndex i = 0; i < json.size(); i++) {
				stream strm;
				for (auto it = json[i].begin(); it != json[i].end(); it++) {
					if (it.key() == "codec_name" && !it->isNull()) strm.codec_name = it->asString();
					else if (it.key() == "channels" && it->isInt()) strm.channels = it->asInt();
					else if (it.key() == "width" && it->isInt()) m_width = it->asInt();
					else if (it.key() == "height" && it->isInt()) m_height = it->asInt();
					else if (it.key() == "tags") {
						if (it->size() > 0 && !(*it).begin()->isNull())
							strm.language = (*it).begin()->asString();
					}
				}
				m_streams[type].push_back(strm);
			}
		}
		catch (const std::exception& e) {
			//Something went wrong but we ignore it
		}
	}
}

std::optional<FFprobe::stream::RESOLUTION> FFprobe::get_resolution() const {
	std::optional<FFprobe::stream::RESOLUTION> res;

	if (m_height) { // Width is ignored
		// The protection is needed because somehow video resolution might not be detected by FFprobe, just in case...
		if (*m_height <= stream::RESOLUTION_MAX_HEIGHT.at(stream::RES_480P))
			res = stream::RES_480P;
		else if (*m_height <= stream::RESOLUTION_MAX_HEIGHT.at(stream::RES_720P))
			res = stream::RES_720P;
		else if (*m_height <= stream::RESOLUTION_MAX_HEIGHT.at(stream::RES_1080P))
			res = stream::RES_1080P;
		else if (*m_height <= stream::RESOLUTION_MAX_HEIGHT.at(stream::RES_4K))
			res = stream::RES_4K;
		else if (*m_height <= stream::RESOLUTION_MAX_HEIGHT.at(stream::RES_8K))
			res = stream::RES_8K;
	}

	return res;
}

#ifdef ENABLE_HEVC
bool FFprobe::is_HDR_detected() const {
	return 	m_red_x && Utils::Input::is_int(*m_red_x) &&
			m_red_y && Utils::Input::is_int(*m_red_y) &&
			m_green_x && Utils::Input::is_int(*m_green_x) &&
			m_green_y && Utils::Input::is_int(*m_green_y) &&
			m_blue_x && Utils::Input::is_int(*m_blue_x) &&
			m_blue_y && Utils::Input::is_int(*m_blue_y) &&
			m_white_point_x && Utils::Input::is_int(*m_white_point_x) &&
			m_white_point_y && Utils::Input::is_int(*m_white_point_y) &&
			m_min_luminance && Utils::Input::is_int(*m_min_luminance) &&
			m_max_luminance && Utils::Input::is_int(*m_max_luminance);
}

bool FFprobe::is_HDR_factible() const {
	return 	m_pix_fmt && m_pix_fmt == "yuv420p10le" &&
			m_color_space && m_color_space == "bt2020nc" &&
			m_color_primaries && m_color_primaries == "bt2020" &&
			m_color_transfer && m_color_transfer == "smpte2084";
}

Stream::Video::HEVC::HDR FFprobe::get_HDR() const {
	Stream::Video::HEVC::HDR result = Stream::Video::HEVC::DEFAULT_HDR;
	if (is_HDR_detected()) {
		// Then were we put the correct values
		result.set_red(std::stoi(*m_red_x), std::stoi(*m_red_y));
		result.set_green(std::stoi(*m_green_x), std::stoi(*m_green_y));
		result.set_blue(std::stoi(*m_blue_x), std::stoi(*m_blue_y));
		result.set_white_point(std::stoi(*m_white_point_x), std::stoi(*m_white_point_y));
		result.set_luminance(std::stoi(*m_min_luminance), std::stoi(*m_max_luminance));
		if (m_max_content && m_max_average) {
			result.set_light_level(std::stoi(*m_max_content), std::stoi(*m_max_average));
		}
	}
	return result;
}
#endif

std::optional<Json::Value> FFprobe::parse_json(const std::string& json) const {
	Json::Reader reader;
    Json::Value root;
	std::optional<Json::Value> result;
    try {
		reader.parse(json, root, false);
		if (root.size() > 0 && root.type() != Json::ValueType::nullValue) result = std::move(root);
	}
	catch (const std::exception&) {
		// Something went wrong but we ignore it
	}
	return result;
}
