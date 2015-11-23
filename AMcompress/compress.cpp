#include "rely.h"
#include "basic.h"
#include "compress.h"
#include "checker.h"
#include "dict.h"
#include "buffer.h"
#include "coder.h"
#include "bitfile.h"


uint8_t acp::compress(wstring filename, cmder set, ProgressInfo &pinfo)
{
	uint8_t *ftDATA = new uint8_t[2048000];
	uint64_t p_ftDATA = 0;
	bool is_dict = set.is_use_dict;
	//try open file
	bitWfile fout;
	FILE *inf = _wfopen(filename.c_str(), L"rb");
	if (inf == NULL)
		return 0x1;
	fseek(inf, 0, SEEK_END);
	pinfo.inlen = ftell(inf);
	fseek(inf, 0, SEEK_SET);

	fread(ftDATA, 1, pinfo.inlen, inf);
	fseek(inf, 0, SEEK_SET);

	wstring ofilename;
	if(is_dict)
		ofilename = filename + L".amc";
	else
		ofilename = L"@" + filename + L".amc";
	if (!fout.open(ofilename))
		return 0x2;
#if DEBUG
	//wchar_t db_str[120];
	db_log(true);
#endif
	//Head Part

	FHeader fhead;
	fhead.diccount = set.dictcount;
	fhead.bufcount = set.bufcount;
	fhead.toData();
	fout.putChars(20, fhead.data);//write file head
	uint8_t fnl = (uint8_t)filename.size();
	fout.putChars(1, &fnl);//file name length
	fout.putChars(fnl * 2, (uint8_t*)filename.c_str());//file name data
	fout.putChars(8, (uint8_t*)&pinfo.inlen);//input file length
	pinfo.outnow = fout.getpos();

	//start

	Dict_init(set.dictcount);
	Buffer_init(set.bufcount);

	uint8_t next_read = 64;
	ChkItem chkdata;
	DictOP dOP;
	BufferOP bOP;
	CoderOP cOP;
	DictReport *dRep = new DictReport[set.thread];
	BufferReport *bRep = new BufferReport[set.thread];

	unique_lock <mutex> lck_DictUse(mtx_Dict_Use);
	thread T_FD = thread(FindInDict, set.thread, ref(dOP), dRep, ref(chkdata));
	dOP.op = 1;
	T_FD.detach();
	//give up lck to enable FindInDict to init
	cv_Dict_Ready.wait(lck_DictUse, [&] {return dOP.op == 0; });

	unique_lock <mutex> lck_BufUse(mtx_Buf_Use);
	thread T_FB = thread(FindInBuffer, set.thread, ref(bOP), bRep, ref(chkdata));
	bOP.op = 1;
	T_FB.detach();
	//give up lck to enable FindInBuffer to init
	cv_Buf_Ready.wait(lck_BufUse, [&] {return bOP.op == 0; });

	/*
	unique_lock <mutex> lck_CoderUse(mtx_Coder_Use);
	thread T_C = thread(CoderT, ref(fout), ref(cOP));
	cOP.op = 1;
	T_C.detach();
	//give up lck to enable Coder to init
	cv_Coder_Ready.wait(lck_CoderUse, [&] {return cOP.op == 0; });
	*/

	dOP.op = bOP.op = 1;
	CodeAns Dans, Bans;
#if DEBUG
	auto chkadddata = [&]
	{
		for (auto a = 0; a < bOP.len; ++a)
		{
			if (ftDATA[p_ftDATA++] != bOP.data[a])
				return false;
		}
		return true;
	};
#endif
	if (is_dict)
	{
		while (Chk_upd(chkdata, inf, next_read) >= 3)//loop untill file end
		{
			Chk_pre(chkdata, 3);
		#if DEBUG
			if (bOP.op == 0xfe)
			{
				if (!chkadddata())
				{//wprintf(L"\nwrong add data!at %d dict-cycle\n",d_cycle);}
			}
		#endif
			//Dict Part
			future<CodeAns> GetDictTestAns;
			cv_Dict_Use.notify_all();
		#if DEBUG_Thr
			db_log(L"M** noti DC0\n");
		#endif
			cv_Dict_Ready.wait(lck_DictUse, [&] { return dOP.op == 0x7f; });
		#if DEBUG_Thr
			db_log(L"M** wa<- DC0\n");
		#endif
			//pre Dict Ans
			GetDictTestAns = async(Code_TestDict, set.thread, dRep);
		#if DEBUG_Thr
			db_log(L"M** make CTD\n");
		#endif

			//prepare data for buffer
			if (dOP.findlen > 3)
				Chk_pre(chkdata, dOP.findlen);

			//Buffer Part
			cv_Buf_Use.notify_all();
		#if DEBUG_Thr
			db_log(L"M** noti BC0\n");
		#endif
			cv_Buf_Ready.wait(lck_BufUse, [&] { return bOP.op == 0x7f; });
		#if DEBUG_Thr
			db_log(L"M** wa<- BC0\n");
		#endif

			//pre Buf Ans
			future<CodeAns> GetBufTestAns = async(Code_TestBuffer, set.thread, bRep);
		#if DEBUG_Thr
			db_log(L"M** make CTB\n");
		#endif

			//get Ans
			Dans = GetDictTestAns.get();

		#if DEBUG_Thr
			db_log(L"M** get CTD\n");
		#endif

			bOP.op = 0xfe;
			Bans = GetBufTestAns.get();
		#if DEBUG_Thr
			db_log(L"M** get CTB\n");
		#endif

		#if DEBUG_Thr
			db_log(L"**Get Ans\n");
		#endif
			//pre next op
			if (Bans.savelen >= Dans.savelen)//Buffer find more&equal byte OR Buffer save more&equal space
			{
				if (Bans.savelen < 0)//no saving bits
				{
					//put one byte and add buffer
					cOP.bdata = chkdata.data[0];
					bOP.len = next_read = 1;
					bOP.data[0] = cOP.bdata;
					cOP.op = 0xfe;
					dOP.op = 0x33;
				}
				else
				{
					//put buffer and add dict and buffer
					cOP.cdata = Bans;
					dOP.op = 0xfe;
					bOP.len = dOP.len = next_read = Bans.srclen;
					memcpy(bOP.data, chkdata.data, next_read);
					memcpy(dOP.data, chkdata.data, next_read);
					//bOP.data = dOP.data = Bans.addr;
					cOP.op = 0xfd;

					dOP.bOffset = Bans.part_data[5];
				}
			}
			else
			{
				//put dict and use dict and add buffer
				cOP.cdata = Dans;
				dOP.op = 0xfd;
				//bOP.data = Dans.addr;
				dOP.len = bOP.len = next_read = Dans.srclen;
				memcpy(bOP.data, Dans.addr, next_read);
				dOP.dID = Dans.dID;
				cOP.op = 0xfd;
			}

			/*//send data to coder
			cv_Coder_Use.notify_all();
			cv_Coder_Ready.wait(lck_CoderUse, [&] {return cOP.op == 0x0; });//give up lock so that coder can do work
			*/
			Coder(fout, cOP);

			//refresh progress-info
			pinfo.innow += next_read;
			pinfo.outnow = fout.pos();
		}
	}
	else
	{//no dict
		while (Chk_upd(chkdata, inf, next_read) >= 3)//loop untill file end
		{
			Chk_pre(chkdata, 3);
		#if DEBUG
			if (bOP.op == 0xfe)
			{
				if (!chkadddata())
				{//wprintf(L"\nwrong add data!at %d dict-cycle\n",d_cycle);}
			}
		#endif
			//Buffer Part
			cv_Buf_Use.notify_all();
		#if DEBUG_Thr
			db_log(L"M** noti BC0\n");
		#endif
			cv_Buf_Ready.wait(lck_BufUse, [&] { return bOP.op == 0x7f; });
		#if DEBUG_Thr
			db_log(L"M** wa<- BC0\n");
		#endif

			//pre Buf Ans
			future<CodeAns> GetBufTestAns = async(Code_TestBuffer, set.thread, bRep);
		#if DEBUG_Thr
			db_log(L"M** make CTB\n");
		#endif

		#if DEBUG_Thr
			db_log(L"M** get CTD\n");
		#endif

			bOP.op = 0xfe;
			Bans = GetBufTestAns.get();
		#if DEBUG_Thr
			db_log(L"M** get CTB\n");
		#endif

		#if DEBUG_Thr
			db_log(L"**Get Ans\n");
		#endif
			//pre next op
			if (Bans.savelen < 0)//no saving bits
			{
				//put one byte and add buffer
				cOP.bdata = chkdata.data[0];
				bOP.len = next_read = 1;
				bOP.data[0] = cOP.bdata;
				cOP.op = 0xfe;
				dOP.op = 0x33;
			}
			else
			{
				//put buffer and add dict and buffer
				cOP.cdata = Bans;
				dOP.op = 0x33;
				bOP.len = dOP.len = next_read = Bans.srclen;
				memcpy(bOP.data, chkdata.data, next_read);
				memcpy(dOP.data, chkdata.data, next_read);
				//bOP.data = dOP.data = Bans.addr;
				cOP.op = 0xfd;
			}

			/*//send data to coder
			cv_Coder_Use.notify_all();
			cv_Coder_Ready.wait(lck_CoderUse, [&] {return cOP.op == 0x0; });//give up lock so that coder can do work
			*/
			Coder(fout, cOP);

			//refresh progress-info
			pinfo.innow += next_read;
			pinfo.outnow = fout.pos();
		}
	}
	if (chkdata.limit)//still little data
	{
		cOP.enddata[0] = chkdata.limit;
		pinfo.innow += chkdata.limit;
		cOP.enddata[1] = chkdata.data[0];
		cOP.enddata[2] = chkdata.data[1];
	}

	//end
	bOP.op = cOP.op = dOP.op = 0xff;
	//end dict
	cv_Dict_Use.notify_all();
	cv_Dict_Ready.wait(lck_DictUse, [&] {return dOP.op == 0x7f; });
	//end buffer
	cv_Buf_Use.notify_all();
	cv_Buf_Ready.wait(lck_BufUse, [&] {return bOP.op == 0x7f; });
	//end coder
	/*
	cv_Coder_Use.notify_all();
	cv_Coder_Ready.wait(lck_CoderUse, [&] {return cOP.op == 0x7f; });//give up lock so that coder can do work
	*/
	Coder(fout, cOP);

	Dict_exit();
	Buffer_exit();
	pinfo.outlen = fout.close();
	fclose(inf);
#if DEBUG
	db_log();
#endif
	return 0x0;
}


