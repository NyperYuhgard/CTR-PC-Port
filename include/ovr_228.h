#ifndef OVR_228_H
#define OVR_228_H

enum OverlayRDATA_228_Counts
{
	OVR228_BUCKET_COUNT = 8,
	OVR228_SCRATCH_INIT_WORD_COUNT = 24,
	OVR228_SETUP_COPY0_WORD_COUNT = 15,
	OVR228_SETUP_COPY1_WORD_COUNT = 3,
	OVR228_CLIP_RECORD_JUMP_WORD_COUNT = 24,
};

enum OverlayRDATA_228_Addresses
{
	OVR228_RDATA_START = 0x800a8e38,
	OVR228_RDATA_BUCKET_SETUP_BASE = 0x800a8ed8,
	OVR228_RDATA_STOP = 0x800a9258,
};

struct OverlayRDATA_228_BucketSetupCopy
{
	u32 loopCounter;
	u32 sourceAddress;
	u32 scratchOffset;
};

struct OverlayRDATA_228_BucketSetupRecord
{
	struct OverlayRDATA_228_BucketSetupCopy copies[2];
	u32 padding;
	u32 copy0[OVR228_SETUP_COPY0_WORD_COUNT];
	u32 copy1[OVR228_SETUP_COPY1_WORD_COUNT];
};

struct OverlayRDATA_228
{
	// 0x800a8e38
	u32 bucketSetupAddresses[OVR228_BUCKET_COUNT];

	// 0x800a8e58
	u32 bucketHandlerAddresses[OVR228_BUCKET_COUNT];

	// 0x800a8e78
	u32 scratchInitTable[OVR228_SCRATCH_INIT_WORD_COUNT];

	// 0x800a8ed8
	struct OverlayRDATA_228_BucketSetupRecord bucketSetups[OVR228_BUCKET_COUNT];

	// 0x800a91f8
	u32 clipRecordJumpTable[OVR228_CLIP_RECORD_JUMP_WORD_COUNT];
};

extern struct OverlayRDATA_228 R228;

#endif
