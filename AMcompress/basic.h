#pragma once

namespace acp
{
	struct ProgressInfo
	{
		_CRT_ALIGN(16) uint64_t inlen = 0L,
			outlen = 0L,
			innow = 0L,
			outnow = 0L;
	};

	class FHeader
	{
	public:
		uint8_t data[20];
		uint8_t IDw[3] = { 0x41,0x43,0x46 };
		uint8_t ver = 0x1;
		uint16_t diccount, bufcount;
		FHeader();
		void toData();
		void toStruct();
	};
}
struct ParseTable
{
	wchar_t raw[16];
	int8_t sig;
	char data;
};


class cmdpaser
{
private:
	int8_t count = 0;
	vector<ParseTable> Ttab;
	int32_t dI[128];
	wstring dS[128];
	int8_t dC[128];
public:
	cmdpaser(vector<ParseTable> &tab);
	void init(int argc, wchar_t *argv[]);
	int8_t size();
	int8_t com(int8_t num);
	wstring dataS(int8_t num);
	int32_t dataI(int8_t num);
};


struct cmder
{
	bool debug;
	bool is_use_dict;
	uint16_t dictcount;
	uint16_t bufcount;
	int8_t thread;
};

void db_log(const wchar_t *str);
bool db_log(const uint8_t channel, const wchar_t* str);
void db_dump(const uint8_t channel, const uint8_t *dat, const uint32_t len);
uint64_t db_com(const wchar_t *str, bool inORout);
void db_log(bool cORu);
void db_log();

void showdata(const uint8_t *inaddr, wchar_t *outaddr, const uint8_t len);

wstring s2ws(const string &in);
void s2ws(const char* in, wchar_t* out);