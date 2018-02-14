
namespace DOWNLOADER
{
	typedef void (*PROGRESS_PROC)(int progress);

    bool Init();
	bool EraseChip();
	bool WriteChip(const BYTE* pbROM, PROGRESS_PROC proc, bool doVerify);
};
