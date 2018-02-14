#include "std.h"
#include "downloader.h"
#include <iostream>
#include <fstream>
#include <cstdio>
using namespace std;

#define LOGFILE	"icburn_logfile.txt" 

static void PrUsage()
{
	printf("\niCybie cartridge writing program, Version 0.2\n");
	printf("Writes an image file to SST flash memory plugged into a SilverLit Downloader using *.ic3 all-in-one binary file and *.bin (lower and upper half inclusion) files.\n\n");
	printf("\tusage:\n");
	printf("\t\ticburn [flags] filename\n");
	printf("\tinput:\n");
	printf("\t\t\n");
	printf("\t\tfull path to file without extension\n");
	printf("\t\tfor example in WindowsXP:\n");
	printf("\t\tc:\\icybie\\icburn\\generic202\n");
	printf("\tflags:\n");
	printf("\t\t-t trust mode (don't verify)\n");
	exit(-1);
}

BYTE g_rgbImage[256 * 1024];    // max size
BYTE g_rgbHalf[128 * 1024];    // for -l/-h loading

void WriteLog(char* message)
{
	ofstream logFile;
	logFile.open(LOGFILE);
	logFile << message;
	logFile.close();
	// Overwrites existing logfile. A better way is...
	//freopen(LOGFILE, "w", message);
	// although not finished.
}

bool ReInstall()
{
	RemoveWinIoDriver();    // Remove the one which was found.

	char szHere[_MAX_PATH];
	//Find directory where .EXE resides.
	{
		char szMod[_MAX_PATH] = {};
		GetModuleFileNameA(NULL, szMod, sizeof(szMod));
		char szDrive[_MAX_DRIVE];
		char szDir[_MAX_DIR];
		_splitpath(szMod, szDrive, szDir, NULL, NULL);
		_makepath(szHere, szDrive, szDir, NULL, NULL);
	}

	if (!InstallWinIoDriver(szHere, false))
	{
		printf("Failed to install WinIo.\n");
		printf("Restart with a console in Administrator mode if using Windows 7 or greater.\n");
		return false;
	}
	printf("WinIo installed.\n");
	return true;
}

bool LoadFile(const char* szInFile, byte* pbImage, int cbImage)
{
	FILE* pfIn = fopen(szInFile, "rb");
	if (pfIn == NULL)
	{
		fprintf(stderr, "Cannot open input file '%s'.\n", szInFile);
		return false;
	}
	fseek(pfIn, 0, SEEK_END);
	int cbFile = ftell(pfIn);
	fseek(pfIn, 0, SEEK_SET);

	if (cbFile != cbImage)
	{
		fclose(pfIn);
		fprintf(stderr, "Input file '%s' is the wrong size.\n", szInFile);
		fprintf(stderr, "\t %d bytes of %d bytes expected.\n", cbFile, cbImage);
		return false;
	}

	if (fread(pbImage, cbImage, 1, pfIn) != 1)
	{
		fclose(pfIn);
		fprintf(stderr, "Read error on input file '%s'.\n", szInFile);
		return false;
	}
	fclose(pfIn);
	return true;
}

void ProgressProc(int progress)
{
	printf(".");
}

int main(int argc, char* argv[])
{
	bool doVerify = true;

	while (argc > 2)
	{
		// process flags
		const char* szFlag = argv[1];
		argv++;
		argc--;

		if (strcmp(szFlag, "-t") == 0)
			doVerify = false;
		else
			PrUsage();  // bad flag
	}

	if (argc != 2)
		PrUsage();

	const char* szInputSource = argv[1]; // file or file prefix
	const int cbImage = 256 * 1024;
	const char* szExt = strchr(szInputSource, '.');

	if (szExt != NULL && strcmpi(szExt, ".ic3") == 0)
	{
		if (!LoadFile(szInputSource, g_rgbImage, cbImage))
			return -1;
	}
	else if (szExt == NULL)
	{
		char szInFile[_MAX_PATH];
		sprintf(szInFile, "%s-l.bin", szInputSource);

		if (!LoadFile(szInFile, g_rgbHalf, cbImage / 2))
			return -1;
		for (int i = 0; i < cbImage / 2; i++)
			g_rgbImage[i * 2] = g_rgbHalf[i];

		sprintf(szInFile, "%s-h.bin", szInputSource);

		if (!LoadFile(szInFile, g_rgbHalf, cbImage / 2))
			return -1;
		for (int i = 0; i < cbImage / 2; i++)
			g_rgbImage[i * 2 + 1] = g_rgbHalf[i];
	}
	else
	{
		printf("\n\n");
		fprintf(stderr, "Cannot determine the file type '%s'\n", szInputSource);
		WriteLog("Cannot determine the file type.");
		exit(-1);
	}
	printf("\n\n");
	printf("Files successfully read. Send to downloader.\n\n");

	if (!InitializeWinIo())
	{
		printf("Failed to startup WinIo. Trying to reinstall.\n");
		if (!ReInstall())
		{
			printf("Reinstall of WinIo failed.\n");
			return -1;
		}
		if (!InitializeWinIo())
		{
			printf("Still failed to startup. Cannot run WinIo.\n");
			return -1;
		}
	}
	printf("Try connecting to downloader.\n");

	if (!DOWNLOADER::Init())
	{
		printf("Downloader connection failed.\n");
		printf("Is it plugged in to the parallel port and turned on?\n");
		WriteLog("Downloader connection failed. Is it plugged in to the parallel port and turned on?");
		Sleep(1000);
		ShutdownWinIo();
		return -1;
	}
	printf("Connected.\n");
	printf("Try erasing the chip.\n");

	// Run twice, lower then upper.
	if (!DOWNLOADER::EraseChip() || !DOWNLOADER::EraseChip())
	{
		printf("Erase failed.\n");
		WriteLog("Erase failed.\n");
		ShutdownWinIo();
		return -1;
	}
	printf("Chip erased.\n");
	printf("Writing files.\n");

	if (!DOWNLOADER::WriteChip(g_rgbImage, ProgressProc, doVerify))
	{
		printf("File write failed.\n");
		WriteLog("File write failed.\n");
		if (doVerify)
		{
			printf("Parallel port many not work bi-directionally. Try '-t' switch\n");
			WriteLog("Parallel port many not work bi-directionally. Try '-t' switch.");
		}
			
		ShutdownWinIo();
		return -1;
	}
	printf("File written.\n");
	WriteLog("File written.");

	ShutdownWinIo();

	return 0;
}
