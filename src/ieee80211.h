#ifndef __IEEE_802_11__
#define __IEEE_802_11__


typedef enum Types {
	TYPE_MANAGEMENT = 0,
	TYPE_CONTROL = 1,
	TYPE_DATA = 2
} Types;

typedef enum DATA_Subtypes {
	DATA_SUBTYPE_QOS = 8,
};

typedef struct FrameControl {
	unsigned int ProtocolVersion : 2;
	unsigned int Type : 2;
	unsigned int Subtype : 4;
	unsigned int ToDS : 1;
	unsigned int FromDS : 1;
	unsigned int MoreFragments : 1;
	unsigned int Retry : 1;
	unsigned int PowerManagement : 1;
	unsigned int MoreData : 1;
	unsigned int ProtectedFrame : 1;
	unsigned int HTC_Order : 1;
} FrameControl;

#endif // __IEEE_802_11__
