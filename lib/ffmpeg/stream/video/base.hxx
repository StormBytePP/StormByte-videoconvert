#pragma once

#include "../base.hxx"

namespace StormByte::VideoConvert::Stream::Video {
	class Base: public StormByte::VideoConvert::Stream::Base {
		public:
			Base(const unsigned short& stream_id, const std::string& encoder, const Database::Data::film::stream::codec& codec);
			Base(unsigned short&& stream_id, std::string&& encoder, Database::Data::film::stream::codec&& codec);
			Base(const Base& base) = default;
			Base(Base&& base) noexcept = default;
			Base& operator=(const Base& base) = default;
			Base& operator=(Base&& base) noexcept = default;
			virtual ~Base() = default;
			
			virtual std::list<std::string> ffmpeg_parameters() const override;
			inline void set_tune_animation() { m_is_animation = true; }
			inline void set_max_rate(const std::string& max_rate) { m_max_rate = max_rate; }

		protected:
			bool m_is_animation;
			std::optional<std::string> m_max_rate;

		private:
			static const bool IS_ANIMATION_DEFAULT;
	};
}