#pragma once

#include "base.hxx"

namespace StormByte::VideoConvert::Stream::Audio {
	class AC3: public Base {
		public:
			AC3(const unsigned short& stream_id);
			AC3(unsigned short&& stream_id);
			AC3(const AC3& ac3) = default;
			AC3(AC3&& ac3) noexcept = default;
			AC3& operator=(const AC3& ac3) = default;
			AC3& operator=(AC3&& ac3) noexcept = default;
			~AC3() = default;
		
		private:
			static const std::string AC3_DEFAULT_ENCODER;

			inline AC3* copy() const { return new AC3(*this); }
	};
}
