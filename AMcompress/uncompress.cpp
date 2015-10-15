#include "rely.h"
#include "basic.h"
#include "uncompress.h"
#include "checker.h"
#include "dict.h"
#include "buffer.h"
#include "coder.h"
#include "bitfile.h"

static FILE *tstf;
static bool isCon = true;

#if DEBUG_Com
#define _fwrite Tfwrite
#else
#define _fwrite fwrite
#endif

static void Tfwrite(void const* _Buffer, size_t      _ElementSize, size_t      _ElementCount, FILE*       _Stream)
{
	uint8_t test[512];
	uint8_t *buf = (uint8_t*)_Buffer;
	fread(test, 1, _ElementCount, tstf);
	for (auto a = 0; a < _ElementCount; a++)
		if (test[a] != buf[a])
			isCon = false;
	fwrite(_Buffer, _ElementSize, _ElementCount, _Stream);
}

uint8_t acp::uncompress(wstring filename, cmder set, ProgressInfo &pinfo)
{
	//try open file
	bitRfile fin;
	FILE *outf = NULL;
	if (!fin.open(filename))
		return 0x1;
	pinfo.inlen = fin.size();

	//Head Part
	FHeader fhead;
	fin.getChars(20, fhead.data);//read file head
	fhead.toStruct();
	uint8_t fnl = 0;
	fin.getChars(1, &fnl);//file name length
	wchar_t tmpoutname[256];
	fin.getChars(fnl * 2, (uint8_t*)tmpoutname);//file name data
	uint8_t *tmpnameaddr = (uint8_t*)tmpoutname + fnl * 2;
	*tmpnameaddr++ = 0x0; *tmpnameaddr = 0x0;
	wstring ofilename = wstring(tmpoutname) + L".txt";
	outf = _wfopen(ofilename.c_str(), L"wb");
	if (!outf)
		return 0x2;
	fin.getChars(8, (uint8_t*)&pinfo.outlen);//input file length
	pinfo.innow = fin.getpos();

#if DEBUG_Com
	wchar_t db_str[120];
	uint64_t cycle = 0;
	db_log(false);
	tstf = _wfopen(tmpoutname, L"rb");
#endif

	//start

	Dict_init(fhead.diccount);
	Buffer_init(fhead.bufcount);
	DecoderOP cOP;
	uint8_t tmp;

	while (DeCoder(fin, cOP))//loop untill file end
	{
		//if (isDumpBuffer)
			//dumpbuffer();
#if DEBUG_Com
		++cycle;
#endif
		switch (cOP.op)
		{
		case 0x1:
			//RAW data
			_fwrite(cOP.data, 1, cOP.len, outf);
			BufAdd(cOP.len, cOP.data);
			break;
		case 0x2:
			//Dict data
			tmp = (uint8_t)cOP.len;
			if (DictGet(cOP.dID, cOP.offset, tmp, cOP.data))
			{
				//error
			}
			else
			{
				cOP.len = tmp;
				_fwrite(cOP.data, 1, cOP.len, outf);
				BufAdd(cOP.len, cOP.data);
			}
			break;
		case 0x3:
			//Buffer data
			if (BufGet(cOP.offset, (uint8_t)cOP.len, cOP.data))
			{
				//error
			}
			else
			{
#if DEBUG_Com
				swprintf(db_str, L"DIC ADD~ from %d\n", cOP.offset);
				db_log(1, db_str);
#endif
				DictAdd((uint8_t)cOP.len, cOP.data);
				_fwrite(cOP.data, 1, cOP.len, outf);
				BufAdd(cOP.len, cOP.data);
			}
			break;
		case 0xff:
			//end
			goto outside;
		case 0x0:
			//error
			wprintf(L"ERROR!\n");
			break;
		}


		//refresh progress-info
		pinfo.innow = fin.getpos();
		pinfo.outnow = ftell(outf);
#if DEBUG_Com
		if (!isCon)
		{
			wprintf(L"wrong here\n"); 
			//dumpbuffer();
			
			break;
		}
#endif
	}

outside:
	//end
	
	Dict_exit();
	Buffer_exit();
	pinfo.outlen = ftell(outf);
	fclose(outf);
	fin.close();
#if DEBUG
	db_log();
#endif
	return 0x0;
}


