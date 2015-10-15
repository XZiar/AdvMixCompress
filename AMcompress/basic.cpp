#include "rely.h"
#include "basic.h"

static FILE *logfile[5];
static atomic_uint32_t lognum[6];
static FILE *datfile[5];

acp::FHeader::FHeader()
{
	memset(data, 0, 20);
}

void acp::FHeader::toData()
{
	memcpy(data, IDw, 3);
	data[3] = ver;
	memcpy(data + 4, &diccount, 2);
	memcpy(data + 6, &bufcount, 2);
}

void acp::FHeader::toStruct()
{
	memcpy(IDw, data, 3);
	ver = data[3];
	memcpy(&diccount, data + 4, 2);
	memcpy(&bufcount, data + 6, 2);
}


cmdpaser::cmdpaser(vector<ParseTable> &tab)
{
	Ttab = tab;
}

void cmdpaser::init(int argc, wchar_t* argv[])
{
	for (auto a = 1; a < argc; a++)
	{
		bool isWrong = true;
		if (argv[a][0] == '-')
		{
			for (auto b = 1; b < Ttab.size(); b++)
			{
				if (wcscmp(Ttab[b].raw, argv[a] + 1) == 0)
				{
					isWrong = false;
					dC[count] = Ttab[b].sig;
					dI[count] = 0;
					dS[count] = L"";
					switch (Ttab[b].data)
					{
					case 'i':
						dI[count] = _wtoi(argv[++a]);
						//printf("here integer:%s and %d\n", argv[a],dI[count]);
						break;
					case 's':
						dS[count] = wstring(argv[++a]);
						break;
					default:
						break;
					}
					break;
				}
			}
		}
		if (isWrong)
		{
			dC[count] = Ttab[0].sig;
			dS[count] = wstring(argv[a]);
		}
		count++;
	}
}

int8_t cmdpaser::size()
{
	return count;
}

int8_t cmdpaser::com(int8_t num)
{
	if (num >= 0 && num < count)
		return dC[num];
	else
		return -1;
}

wstring cmdpaser::dataS(int8_t num)
{
	if (num >= 0 && num < count)
		return dS[num];
	else
		return L"";
}

int32_t cmdpaser::dataI(int8_t num)
{
	if (num >= 0 && num < count)
		return dI[num];
	else
		return -1;
}



void db_log(const wchar_t *str)
{
	fwprintf(logfile[0], str);
	//fflush(logfile[0]);
}

bool db_log(const uint8_t channel, const wchar_t *str)
{
	fwprintf(logfile[channel], str);
	return ++lognum[channel] >= lognum[5];

}

void db_dump(const uint8_t channel, const uint8_t *dat, const uint32_t len)
{
	fwrite(dat, 1, len, datfile[channel]);
	fflush(datfile[channel]);
}
static uint64_t cycle = 0;
uint64_t db_com(const wchar_t *str, bool inORout)
{
	fwprintf(datfile[0], str);
	return ++cycle;
}

void db_log(bool cORu)
{
	if (cORu)//compress
	{
		datfile[0] = _wfopen(L"COM-out.txt", L"w");
		datfile[1] = _wfopen(L"dat1-out.txt", L"wb");
		datfile[2] = _wfopen(L"dat2-out.txt", L"wb");
		logfile[0] = _wfopen(L"logf0-out.txt", L"w");
		logfile[1] = _wfopen(L"logf1-out.txt", L"w");
		logfile[2] = _wfopen(L"logf2-out.txt", L"w");
		logfile[3] = _wfopen(L"logf3-out.txt", L"w");
		logfile[4] = _wfopen(L"logf4-out.txt", L"w");
	}
	else//uncompress
	{
		datfile[0] = _wfopen(L"COM-in.txt", L"w");
		datfile[1] = _wfopen(L"dat1-in.txt", L"wb");
		datfile[2] = _wfopen(L"dat2-in.txt", L"wb");
		logfile[0] = _wfopen(L"logf0-in.txt", L"w");
		logfile[1] = _wfopen(L"logf1-in.txt", L"w");
		logfile[2] = _wfopen(L"logf2-in.txt", L"w");
		logfile[3] = _wfopen(L"logf3-in.txt", L"w");
		logfile[4] = _wfopen(L"logf4-in.txt", L"w");
	}
	for (auto a = 0; a < 5; a++)
		lognum[a] = 0;
}

void db_log()
{
	fclose(datfile[0]);
	fclose(datfile[1]);
	fclose(datfile[2]);
	fclose(logfile[0]);
	fclose(logfile[1]);
	fclose(logfile[2]);
	fclose(logfile[3]);
	fclose(logfile[4]);

}

void showdata(const uint8_t *inaddr, wchar_t *outaddr, const uint8_t len)
{
	//return;
	uint8_t *oaddr = (uint8_t*)outaddr;
	for (auto a = 0; a < len; ++a)
	{
		*oaddr++ = *inaddr++;
		*oaddr++ = 0x0;
	}
	*oaddr++ = 0x0;
	*oaddr = 0x0;
	return;
}

wstring s2ws(const string &in)
{
	printf("by out:%s\n", in.c_str());
	std::locale old_loc = std::locale::global(std::locale(""));

	const char* src_str = in.c_str();
	const size_t buffer_size = in.size() + 1;
	wchar_t* dst_wstr = new wchar_t[buffer_size];
	wmemset(dst_wstr, 0, buffer_size);
	mbstowcs(dst_wstr, src_str, 2 * buffer_size);
	wstring out = dst_wstr;

	wprintf(L"by wout:%ls\n", dst_wstr);
	delete[]dst_wstr;

	std::locale::global(old_loc);
	return out;
}

void s2ws(const char* in,wchar_t* out)
{
	printf("by out:%s\n", in);
	std::locale old_loc = std::locale::global(std::locale(""));

	const size_t buffer_size = strlen(in)+1;

	wmemset(out, 0, buffer_size);
	mbstowcs(out, in, 2 * buffer_size);

	wprintf(L"by wout:%ls\n", out);

	std::locale::global(old_loc);
	return;
}
