#ifndef OVR_229_H
#define OVR_229_H

enum OverlayRDATA_229_Counts
{
	OVR229_BUCKET_COUNT = 8,
	OVR229_SCRATCH_INIT_WORD_COUNT = 24,
	OVR229_SETUP_COPY0_WORD_COUNT = 15,
	OVR229_SETUP_COPY1_WORD_COUNT = 3,
	OVR229_CLIP_RECORD_JUMP_WORD_COUNT = 24,
};

enum OverlayRDATA_229_Addresses
{
	OVR229_RDATA_START = 0x800a8eec,
	OVR229_RDATA_BUCKET_SETUP_BASE = 0x800a8f8c,
	OVR229_RDATA_STOP = 0x800a930c,
};

struct OverlayRDATA_229_BucketSetupCopy
{
	u32 loopCounter;
	u32 sourceAddress;
	u32 scratchOffset;
};

struct OverlayRDATA_229_BucketSetupRecord
{
	struct OverlayRDATA_229_BucketSetupCopy copies[2];
	u32 padding;
	u32 copy0[OVR229_SETUP_COPY0_WORD_COUNT];
	u32 copy1[OVR229_SETUP_COPY1_WORD_COUNT];
};

struct OverlayRDATA_229
{
	// 0x800a8eec
	u32 bucketSetupAddresses[OVR229_BUCKET_COUNT];

	// 0x800a8f0c
	u32 bucketHandlerAddresses[OVR229_BUCKET_COUNT];

	// 0x800a8f2c
	u32 scratchInitTable[OVR229_SCRATCH_INIT_WORD_COUNT];

	// 0x800a8f8c
	struct OverlayRDATA_229_BucketSetupRecord bucketSetups[OVR229_BUCKET_COUNT];

	// 0x800a92ac
	u32 clipRecordJumpTable[OVR229_CLIP_RECORD_JUMP_WORD_COUNT];
};

extern struct OverlayRDATA_229 R229;

#endif
