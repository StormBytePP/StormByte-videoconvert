#pragma once

#include "base.hxx"

namespace StormByte::VideoConvert::Stream::Audio {
	class Copy: public Base {
		public:
			Copy(unsigned short stream_id);
			Copy(const Copy& copy);
			Copy(Copy&& copy) noexcept;
			Copy& operator=(const Copy& copy);
			Copy& operator=(Copy&& copy) noexcept;
			~Copy() = default;

			std::list<std::string> ffmpeg_parameters() const override;

		private:
			Copy* copy() const override;
	};
}