#pragma once

#include "base.hxx"

namespace StormByte::VideoConvert::Stream::Audio {
	class AAC: public Base {
		public:
			AAC(const unsigned short& stream_id);
			AAC(unsigned short&& stream_id);
			AAC(const AAC& aac) = default;
			AAC(AAC&& aac) noexcept = default;
			AAC& operator=(const AAC& aac) = default;
			AAC& operator=(AAC&& aac) noexcept = default;
			~AAC() = default;

		private:
			static const std::string AAC_DEFAULT_ENCODER;

			inline AAC* copy() const override { return new AAC(*this); }
	};
}
