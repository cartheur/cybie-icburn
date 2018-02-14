#include "std.h"
#include <time.h>
#include "downloader.h"

///////////////////////////////////////////////////////////////////////
// port i/o

#define    Do_strobeL      _toggle_clear(1)  //pin1  sunplus
#define    Do_strobeH      _toggle_set(1)
#define    Do_resetH       _toggle_clear(4)  //pin16 sunplus
#define    Do_resetL       _toggle_set(4)  
#define    Do_latchAH      _toggle_clear(2)  //pin14
#define    Do_latchAL      _toggle_set(2)
#define    Do_latchBH      _toggle_clear(8)  //pin17
#define    Do_latchBL      _toggle_set(8)
#define    Do_asIP     _toggle_set(0x20)     //enable bi-directional
#define    Do_asOP     _toggle_clear(0x20)   //disable bi-directional

void WriteLog(char* message);

static BYTE inline _inp(WORD port_addr)
{
	DWORD dw;
	GetPortVal(port_addr, &dw, 1);
	return (BYTE)dw;
}

static void inline _outp(WORD port_addr, BYTE val)
{
	SetPortVal(port_addr, val, 1);
}

static BYTE g_bPort37a;

static void inline _toggle_set(BYTE val)
{
	g_bPort37a |= val;
	SetPortVal(0x37a, g_bPort37a, 1);
}

static void inline _toggle_clear(BYTE val)
{
	g_bPort37a &= ~val;
	SetPortVal(0x37a, g_bPort37a, 1);
}

///////////////////////////////////////////////////////////////////////

#define TIMEOUT_VALUE   100000  /* hopefully long enough on even fast CPUs */

static bool Strobe()
{
	int timeout = TIMEOUT_VALUE;
	Do_strobeL;
	//wait ACK low pin10
	while (_inp(0x379) & 0x40)
	{
		if (--timeout == 0)
			return false;
	}
	Do_strobeH;
	//wait ACK high
	while (!(_inp(0x379) & 0x40))
	{
		if (--timeout == 0)
			return false;
	}
	return true;
}

static void SendOne(WORD addr, BYTE data)
{
	_outp(0x378, LOBYTE(addr));
	Do_latchAH;
	Do_latchAL;
	_outp(0x378, HIBYTE(addr));
	Do_latchBH;
	Do_latchBL;
	_outp(0x378, data);
}

bool DOWNLOADER::Init()
{
	g_bPort37a = _inp(0x37a);

	Do_strobeH;
	Do_resetH;
	Sleep(100);   //   /delay 150ms for Do_reset CPU
	Do_resetL;
	Sleep(50);     //  delay 150ms for  CPU initialize

	for (int i = 0; i < 4; i++)
	{
		if (i == 0 || i == 1)
			_outp(0x378, 0x01);
		else
			_outp(0x378, 0x00);

		if (!Strobe())
			return false;
	}
	Do_asOP;
	return true;
}

bool DOWNLOADER::EraseChip()
{
	Do_latchAL;
	Do_latchBL;
	Do_strobeH;
	_outp(0x378, 0x55);
	Do_latchAH;
	Do_latchAH; // REVIEW: why twice ?
	Do_latchAL;
	_outp(0x378, 0x55);
	Do_latchBH;
	Do_latchBH;
	Do_latchBL;
	_outp(0x378, 0x0AA);
	if (!Strobe())
		return false;

	_outp(0x378, 0xAA);
	Do_latchAH;
	Do_latchAH;
	Do_latchAL;
	_outp(0x378, 0x2A);
	Do_latchBH;
	Do_latchBH;
	Do_latchBL;
	_outp(0x378, 0x55);
	if (!Strobe())
		return false;

	_outp(0x378, 0x55);
	Do_latchAH;
	Do_latchAH;
	Do_latchAL;
	_outp(0x378, 0x55);
	Do_latchBH;
	Do_latchBH;
	Do_latchBL;
	_outp(0x378, 0x80);
	if (!Strobe())
		return false;

	_outp(0x378, 0x55);
	Do_latchAH;
	Do_latchAH;
	Do_latchAL;
	_outp(0x378, 0x55);
	Do_latchBH;
	Do_latchBH;
	Do_latchBL;
	_outp(0x378, 0x0AA);
	if (!Strobe())
		return false;

	_outp(0x378, 0xAA);
	Do_latchAH;
	Do_latchAH;
	Do_latchAL;
	_outp(0x378, 0x2A);
	Do_latchBH;
	Do_latchBH;
	Do_latchBL;
	_outp(0x378, 0x55);
	if (!Strobe())
		return false;

	_outp(0x378, 0x55);
	Do_latchAH;
	Do_latchAH;
	Do_latchAL;
	_outp(0x378, 0x55);
	Do_latchBH;
	Do_latchBH;
	Do_latchBL;
	_outp(0x378, 0x10);
	if (!Strobe())
		return false;

	return true;
}

bool DOWNLOADER::WriteChip(const BYTE* pbROM, PROGRESS_PROC proc, bool doVerify)
{
	// LOW part first, HIGH part second
	int progress = 0;
	for (int half = 0; half < 2; half++)
	{
		WORD addrToWrite = 0;

		const BYTE* pbHalf = pbROM + half;

		for (int j = 0; j < 128 * 1024; j++)
		{
			BYTE bToWrite = *pbHalf;
			pbHalf += 2;

			SendOne(0x5555, 0xAA);
			if (!Strobe())
				return false;

			SendOne(0x2AAA, 0x55);
			if (!Strobe())
				return false;

			SendOne(0x5555, 0xA0);
			if (!Strobe())
				return false;

			SendOne(addrToWrite, bToWrite);
			addrToWrite++;
			if (!Strobe())
				return false;

			Do_asIP;  // bi-directional read

			Do_strobeL;

			int timeout = TIMEOUT_VALUE;
			// wait ACK low, here die loop
			while (_inp(0x379) & 0x40)
			{
				if (--timeout == 0)
					return false;
			}

			// check to see if anything is read
			_outp(0x378, 0xFF); // force bi-direction
			BYTE bCheck = (_inp(0x378) & 0xff);
			if (bCheck != bToWrite && doVerify)
			{
				WriteLog("Verify failed: $%02X != $%02X\n");
				if (bCheck == 0xFF)
					WriteLog("Parallel port may not be set for bi-directional operation\n");
#if 0
				printf("Verify failed: $%02X != $%02X\n",
					bCheck, bToWrite);
				if (bCheck == 0xFF)
					printf(" (parallel port may not be set for bi-directional\n");
#endif
				return false;
			}
			Do_strobeH;

			while (!(_inp(0x379) & 0x40))
			{
				if (--timeout == 0)
					return false;
			}
			Do_asOP;

			if ((j & 0x3FF) == 0x3FF)
				(*proc)(progress++);
		}
	}

	return true;
}
