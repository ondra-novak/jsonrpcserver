#include "abstractWebSockets.tcc"
#include "lightspeed\base\memory\dynobject.h"
#include "lightspeed\base\streams\secureRandom.h"

namespace BredyHttpSrv {

	namespace {
		class WSDisableMasking: public IWebSocketMasking, public DynObject {
		public:
			virtual void setMaskingBytes(byte *mask)
			{
				mask[0] = mask[1] = mask[2] = mask[3] = 0;
			}
		};

		class WSSecureMasking : public IWebSocketMasking, public DynObject {
		public:
			virtual void setMaskingBytes(byte *mask) {
				sr.blockRead(mask, 4);
			}
		protected:
			SecureRandom sr;
		};


		class WSXorShift64: public IWebSocketMasking, public DynObject {
		public:

			WSXorShift64() {
				x = 0;
				SecureRandom sr;
				while (x == 0) {
					sr.blockRead(&x, sizeof(x));
				}				
			}

			virtual void setMaskingBytes(byte *mask) {
				
				x ^= x >> 12; // a
				x ^= x << 25; // b
				x ^= x >> 27; // c
				uint64_t res = x * 2685821657736338717LL;
				memcpy(mask, &res, 4);
			}


		protected:
			uint64_t x;
		};

	};

	IWebSocketMasking * IWebSocketMasking::getDisabledMasking()
	{
		return getDisabledMasking(StdAlloc::getInstance());
	}

	IWebSocketMasking * IWebSocketMasking::getDisabledMasking(IRuntimeAlloc &alloc)
	{
		return new(alloc) WSDisableMasking;
	}

	IWebSocketMasking * IWebSocketMasking::getSecureMasking()
	{
		return getSecureMasking(StdAlloc::getInstance());
	}

	IWebSocketMasking * IWebSocketMasking::getSecureMasking(IRuntimeAlloc &alloc)
	{
		return new(alloc)WSSecureMasking;
	}

	IWebSocketMasking * IWebSocketMasking::getFastMasking()
	{ 
		return getFastMasking(StdAlloc::getInstance());
	}

	IWebSocketMasking * IWebSocketMasking::getFastMasking(IRuntimeAlloc &alloc)
	{
		return new(alloc)WSXorShift64;
	}


}