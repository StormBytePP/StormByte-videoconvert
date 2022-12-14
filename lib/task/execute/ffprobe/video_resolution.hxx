#pragma once

#include "base.hxx"

namespace StormByte::VideoConvert::Task::Execute::FFprobe {
	class VideoResolution: public FFprobe::Base {
		public:
			VideoResolution(const Types::path_t& file);
			VideoResolution(Types::path_t&& file);
			VideoResolution(const VideoResolution&) = default;
			VideoResolution(VideoResolution&&) noexcept = default;
			VideoResolution& operator=(const VideoResolution&) = default;
			VideoResolution& operator=(VideoResolution&&) noexcept = default;
			~VideoResolution() noexcept = default;

		private:
			STATUS pre_run_actions() noexcept override;

			static const std::list<std::string> BASE_ARGUMENTS;
	};
}