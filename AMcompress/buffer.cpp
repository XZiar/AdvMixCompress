#include "rely.h"
#include "basic.h"
#include "checker.h"
#include "buffer.h"
#include "bitfile.h"

namespace acp
{
#define BLKSIZE 8192
#define BLKMASK 0x1fff
	struct alignas(64) BlockInfo
	{
		int16_t hash[832];
		uint16_t jump[BLKSIZE];
	};
	
	static uint8_t *buffer, *rbuffer;
	static BlockInfo *BlkInfo;//
	int16_t Buf_Blk_start,//start block of buf
		Buf_Blk_cur,//current block of buf
		Buf_Blk_max;//max block num of buf
	uint32_t Buf_Pos_cur,//len of buf
		Buf_Pos_max;//max size of buf pool
	static uint64_t judgenum_all[65];//judge num

	mutex mtx_Buf_Use;
	static mutex mtx_FindThread_Wait,//
		mtx_CtrlThread_Wait;//

	condition_variable cv_Buf_Use,
		cv_Buf_Ready;
	static condition_variable cv_FindThread_Wait,//cv to wake up find thread
		cv_CtrlThread_Wait;//cv to wake up ctrl thread
	static atomic_int64_t a_FT_state(0);
	static atomic_int8_t //a_ThreadCount(0),
		a_FindLen(0);
	static atomic_int32_t a_LoopCount(0);

	static FILE *ttf;
	static void dump_buf()
	{
		FILE * tmpf = _wfopen(L"dump_buf.data", L"wb");
		fwrite(buffer, 1, Buf_Pos_cur, tmpf);
		fclose(tmpf);
	}

	void Buffer_init(uint16_t blkcount)
	{
#if DEBUG
		ttf = _wfopen(L"out_buf.data", L"wb");
		fclose(ttf);
#endif
		uint32_t bufcount = blkcount * BLKSIZE;
		Buf_Pos_max = bufcount * 3;

		rbuffer = (uint8_t*)malloc_align(sizeof(uint8_t)*Buf_Pos_max + 64, 64);
		memset(rbuffer, 0x0, 64);
		buffer = rbuffer + 64;
		//buffer = new uint8_t[Buf_Pos_max];
		BlkInfo = (BlockInfo*)malloc_align(sizeof(BlockInfo)*blkcount * 3, 64);
		//BlkInfo = new BlockInfo[blkcount * 3];

		Buf_Blk_cur = Buf_Blk_start = 0;
		Buf_Blk_max = blkcount;

		uint8_t judgetmp[8];
		memset(judgetmp, 0, 8);
		memset(judgenum_all, 0xff, 520);
		for (int a = 0; a < 8; a++)
		{
			judgenum_all[a] = *(uint64_t*)judgetmp;
			judgetmp[a] = 0xff;
		}
		return;
	}

	void Buffer_exit()
	{
#if DEBUG
		ttf = _wfopen(L"out_buf.data", L"ab");
		fwrite(buffer, 1, Buf_Pos_cur, ttf);
		fclose(ttf);
#endif
		free_align(rbuffer);
		//delete[] buffer;
		free_align(BlkInfo);
		//delete[] BlkInfo;
		return;
	}

	static inline void BufClean()
	{
#if DEBUG
		ttf = _wfopen(L"out_buf.data", L"ab");
		fwrite(buffer, 1, (Buf_Blk_max * 3 - Buf_Blk_start) * BLKSIZE, ttf);
		fclose(ttf);
#endif
		void* src = buffer + Buf_Blk_start * BLKSIZE;
		memcpy(buffer, src, (Buf_Blk_max * 3 - Buf_Blk_start) * BLKSIZE);

		src = &BlkInfo[Buf_Blk_start];
		uint32_t len = sizeof(BlockInfo)*Buf_Blk_max;
		memcpy(&BlkInfo[0], src, len);
		Buf_Blk_cur -= Buf_Blk_start;
		Buf_Pos_cur -= Buf_Blk_start * BLKSIZE;
		Buf_Blk_start = 0;
		return;
	}

	static inline void BufPre(BlockInfo &bufinfo)
	{
		memset(bufinfo.hash, 0x80, BLKSIZE * 2);
		return;
	}

	void BufAdd(uint16_t len, uint8_t data[])
	{
		_mm_prefetch((char*)data, _MM_HINT_NTA);
		if (Buf_Pos_cur + len > Buf_Pos_max)//no enough space
			BufClean();
		uint8_t *objaddr = buffer + Buf_Pos_cur;
		memcpy(objaddr, data, len);
		uint16_t left_len = BLKSIZE - (Buf_Pos_cur - Buf_Blk_cur * BLKSIZE),
			right_len = 0;
		if (len > left_len)
		{//full of a block
			right_len = len - left_len;
		}
		else
		{//not full
			left_len = len;
		}

		//fill this block
		for (auto a = Buf_Pos_cur & BLKMASK; left_len--; ++a)
		{
			//auto tmpdat = (uint16_t)buffer[(int32_t)Buf_Pos_cur - 1] * 13 + buffer[(int32_t)Buf_Pos_cur - 2] * 169;
			//tmpdat += buffer[Buf_Pos_cur++];
			auto tmpdat = hash(&buffer[(int32_t)Buf_Pos_cur++]);
			BlkInfo[Buf_Blk_cur].jump[a] = BlkInfo[Buf_Blk_cur].hash[tmpdat = tmpdat % 769];
			BlkInfo[Buf_Blk_cur].hash[tmpdat] = a;
		}

		//fill next block
		while (right_len > 0)
		{
			BufPre(BlkInfo[++Buf_Blk_cur]);
			if (Buf_Blk_cur - Buf_Blk_start >= Buf_Blk_max)
				++Buf_Blk_start;
			if (right_len > BLKSIZE)
			{
				left_len = BLKSIZE;
				right_len -= BLKSIZE;
			}
			else
			{
				left_len = right_len;
				right_len = 0;
			}
			for (auto a = 0; a < left_len; ++a)
			{
				//uint16_t tmpdat = (uint16_t)buffer[(int32_t)Buf_Pos_cur - 1] * 13 + buffer[(int32_t)Buf_Pos_cur - 2] * 169;
				//tmpdat += buffer[Buf_Pos_cur++];
				auto tmpdat = hash(&buffer[(int32_t)Buf_Pos_cur++]);
				BlkInfo[Buf_Blk_cur].jump[a] = BlkInfo[Buf_Blk_cur].hash[tmpdat = tmpdat % 769];
				BlkInfo[Buf_Blk_cur].hash[tmpdat] = a;
			}
		}
		

		return;
	}

	uint8_t BufGet(uint32_t offset, uint8_t len, uint8_t data[])
	{
		//if (offset > Buf_Pos_cur - Buf_Blk_start * BLKSIZE)
		if (offset > Buf_Pos_cur)
			return 0x1;
		if (len > 64)
			return 0x2;
		memcpy(data, buffer + (Buf_Pos_cur - offset), len);
		return 0x0;
	}

	static void FindThread_x64(uint8_t tNum, int8_t tID, uint8_t &op, ChkItem *inchk, BufferReport &bufrep)
	{
		_CRT_ALIGN(16) ChkItem chkdata;
		BlockInfo *blkinf;
		uint64_t *p_buf_cur,//current pos of dic_data
			*p_chk_cur;//current pos of chker_data
		char *p_prefetch;//for prefetch
		int16_t blk_num,//current searching block num
			blk_num_next;
		int32_t findpos,
			bufspos;//real start pos of buf in the whole pool
		int32_t c_thr_left,//left border for this cycle
			c_thr_min,//min border for this cycle
			c_thr_left_next,//left border for this cycle
			c_thr_min_next;//min border for this cycle
		uint8_t chkleft;//left count of checker
		uint16_t &chk_minvalD = chkdata.minvalD;
		uint8_t	&chk_minpos = chkdata.minpos;
		int16_t objpos,
			maxpos,
			maxpos_next;
		_CRT_ALIGN(16) uint64_t judgenum[65];//judge num

		//init
		const uint64_t mask = 0x1Ui64 << tID;
		unique_lock <mutex> lck(mtx_FindThread_Wait);
#if DEBUG_Thr
		wchar_t msg[6][24];

		//prepare msg
		swprintf(msg[0], L"Buf_FThr %2d creat\n", tID);
		swprintf(msg[1], L"Buf_FThr %2d init\n", tID);
		swprintf(msg[2], L"Buf_FThr %2d lock\n", tID);
		swprintf(msg[3], L"Buf_FThr %2d unlock\n", tID);
		swprintf(msg[4], L"BFT %2d noti BC0\n", tID);
		swprintf(msg[5], L"BFT %2d wa<- BC0\n", tID);

		db_log(msg[0]);
#endif
		a_FT_state -= mask;
#if DEBUG_Thr
		db_log(msg[4]);
#endif
		cv_CtrlThread_Wait.notify_all();

		cv_FindThread_Wait.wait(lck, [=] {return a_FT_state & mask; });
#if DEBUG_Thr
		db_log(msg[5]);
#endif
		lck.unlock();
#if DEBUG_Thr
		db_log(msg[3]);
#endif
		memcpy(judgenum, judgenum_all, 520);
#if DEBUG_Thr
		db_log(msg[1]);
#endif
		//prefetch chkdata
		_mm_prefetch((char*)chkdata.data, _MM_HINT_T0);//data
		_mm_prefetch((char*)chkdata.data + 64, _MM_HINT_T0);//propety

		//prefetch judge
		_mm_prefetch((char*)judgenum, _MM_HINT_T0);
		_mm_prefetch((char*)judgenum + 64, _MM_HINT_T0);
		_mm_prefetch((char*)judgenum + 128, _MM_HINT_T0);
		_mm_prefetch((char*)judgenum + 192, _MM_HINT_T0);
		_mm_prefetch((char*)judgenum + 256, _MM_HINT_T0);
		_mm_prefetch((char*)judgenum + 320, _MM_HINT_T0);
		_mm_prefetch((char*)judgenum + 384, _MM_HINT_T0);
		_mm_prefetch((char*)judgenum + 448, _MM_HINT_T0);

		//main part
		auto func_fetch = [&]
		{
			//prefetch buffer data
			_mm_prefetch((char*)buffer + c_thr_left, _MM_HINT_T0);
			_mm_prefetch((char*)buffer + c_thr_left + 64, _MM_HINT_T0);
			//prefetch jump data
			_mm_prefetch((char*)blkinf->jump, _MM_HINT_T0);
			_mm_prefetch((char*)blkinf->jump + 64, _MM_HINT_T0);
		};
		auto func_catchnext = [&]
		{//go to choose next block
			if ((blk_num_next = blk_num - tNum) < Buf_Blk_start)
				return;

			//prefetch
			char *p_hash = (char*)&BlkInfo[blk_num_next].hash[chk_minvalD];//pos of the index of next DictItem
			_mm_prefetch(p_hash, _MM_HINT_NTA);
			while (*(int16_t*)p_hash == (int16_t)0x8080)//no find in this block
			{

				blk_num_next -= tNum;
				if (blk_num_next >= Buf_Blk_start)//avalible
				{
					//prefetch next
					p_hash -= sizeof(BlockInfo)*tNum;//pos of the index of next DictItem
					_mm_prefetch(p_hash, _MM_HINT_NTA);
					continue;
				}
				else
					return;
			}
			//find in this block

			c_thr_left_next = blk_num_next * BLKSIZE;
			int16_t tmp = *(int16_t*)p_hash;
			//prefetch jump data
			_mm_prefetch((char*)&BlkInfo[blk_num_next].jump[tmp], _MM_HINT_T0);
			//prefetch buffer data
			_mm_prefetch((char*)buffer + c_thr_left_next + tmp, _MM_HINT_T0);

			//prepare border
			c_thr_min_next = c_thr_left_next - 64;
			maxpos_next = BLKSIZE + chk_minpos - chkdata.curlen;

		};
		
		while (true)//run one cycle at a FindInBuf
		{
			//refresh chker
			memcpy(&chkdata, inchk, sizeof(ChkItem));
			blk_num = Buf_Blk_cur - tID;

			//locate buffer
			blkinf = &BlkInfo[blk_num];
			func_catchnext();

			//prefetch current
			p_prefetch = (char *)&blkinf->hash[chk_minvalD];//pos of the index of the DictItem
			_mm_prefetch(p_prefetch, _MM_HINT_NTA);

			//prepare border
			c_thr_left = blk_num * BLKSIZE;
			c_thr_min = c_thr_left - 64;
			if(Buf_Blk_cur == blk_num)
				maxpos = (Buf_Pos_cur & BLKMASK) + chk_minpos - chkdata.curlen;
			else
				maxpos = BLKSIZE + chk_minpos - chkdata.curlen;

			func_catchnext();
			//prefetch
			p_prefetch = (char *)&BlkInfo[blk_num].hash[chk_minvalD];//pos of the index of next DictItem
			_mm_prefetch(p_prefetch, _MM_HINT_T0);
			//prefetch jump data
			_mm_prefetch((char*)BlkInfo[blk_num].jump, _MM_HINT_T0);
			_mm_prefetch((char*)BlkInfo[blk_num].jump + 64, _MM_HINT_T0);
			//prefetch buffer data
			_mm_prefetch((char*)buffer + c_thr_left, _MM_HINT_T0);
			_mm_prefetch((char*)buffer + c_thr_left + 64, _MM_HINT_T0);

			for (; blk_num >= Buf_Blk_start;)//loop when finish a BufBlock,fail OR suc(add chk)
			{
				//func_fetch();
				findpos = 0x7fffffff;
				
				objpos = blkinf->hash[chk_minvalD];//object-bit pos in the buf block
				if (objpos == (int8_t)0x8080)//no matching word
				{
					if (blk_num_next < Buf_Blk_start)//no next
						break;
					blkinf = &BlkInfo[blk_num = blk_num_next];
					maxpos = maxpos_next;
					c_thr_left = c_thr_left_next;
					c_thr_min = c_thr_min_next;
					func_catchnext();

					continue;
				}
				while (objpos > maxpos)
					objpos = blkinf->jump[objpos];
				bufspos = c_thr_left + objpos - chk_minpos;//get real start pos
				//if objpos = 0x8080,bufspos must be lefter than c_thr_left more than 128
				//so it must < c_thr_min
				if (bufspos < c_thr_min)//no matching word
				{
					if (blk_num_next < Buf_Blk_start)//no next
						break;
					blkinf = &BlkInfo[blk_num = blk_num_next];
					maxpos = maxpos_next;
					c_thr_left = c_thr_left_next;
					c_thr_min = c_thr_min_next;
					func_catchnext();

					continue;
				}

				p_buf_cur = (uint64_t*)(buffer + bufspos);
				p_chk_cur = (uint64_t*)chkdata.data;
				chkleft = chkdata.curlen;

				while (true)
				{
					//judge part
					if (((*p_buf_cur) ^ (*p_chk_cur)) & judgenum[chkleft])
					{//not match
						objpos = blkinf->jump[objpos];//get next pos
						
						bufspos = c_thr_left + objpos - chk_minpos;//get real start pos

						if (bufspos < c_thr_min)//outside the block OR beyond buffer pool
							break;

						p_buf_cur = (uint64_t*)(buffer + bufspos);
						_mm_prefetch((char*)p_buf_cur, _MM_HINT_T0);
						_mm_prefetch((char*)&blkinf->jump[objpos], _MM_HINT_T0);

						//if (chkleft != chkdata.curlen)
						{//in a match, reset chk pos
							p_chk_cur = (uint64_t*)chkdata.data;
							chkleft = chkdata.curlen;
						}

						continue;
					}//end of not match
					else
					{//match from the beginning
						if (chkleft < 9)
						{//match suc
							findpos = bufspos;
							break;
						}
						else
						{//add more match
							++p_buf_cur, ++p_chk_cur;
							chkleft -= 8;
							continue;
						}
					}//end of match

				}
				//end of dead loop to keep searching in a dict

				if (findpos != 0x7fffffff)//finally find it, update bufrep
				{
				#if DEBUG
					bufrep.p_b = buffer;
					bufrep.s_b = Buf_Pos_cur;
				#endif
					bufrep.isFind = 0xff;
					bufrep.objlen = chkdata.curlen;
					bufrep.addr = buffer + findpos;
					//bufrep.addr = buffer + c_thr_left + findpos;
					bufrep.offset = Buf_Pos_cur - findpos;
					if (Chk_inc(chkdata) == 0)
						break;//chkdata is full used
					//add chk suc
					if (blk_num == Buf_Blk_cur)
					{
						maxpos = (Buf_Pos_cur & BLKMASK) + chk_minpos - chkdata.curlen;
						if (maxpos < 0)//no enough space in this block
						{
							if (blk_num_next < Buf_Blk_start)//no next
								break;
							blkinf = &BlkInfo[blk_num = blk_num_next];
							maxpos = maxpos_next;
							c_thr_left = c_thr_left_next;
							c_thr_min = c_thr_min_next;
							func_catchnext();

						}
					}
					else
						maxpos = BLKSIZE + chk_minpos - chkdata.curlen;
				}
				else
				{
					if (blk_num_next < Buf_Blk_start)//no next
						break;
					blkinf = &BlkInfo[blk_num = blk_num_next];
					maxpos = maxpos_next;
					c_thr_left = c_thr_left_next;
					c_thr_min = c_thr_min_next;
					func_catchnext();

				}
			}
			//end of seaching the whole diction

			//send back signal to the control thread
			lck.lock();
#if DEBUG_Thr
			db_log(msg[2]);
#endif
			a_FT_state -= mask;
			//if (bufrep.isFind && a_FindLen < bufrep.objlen)
				//a_FindLen = bufrep.objlen;
#if DEBUG_Thr
			db_log(msg[4]);
#endif
			cv_CtrlThread_Wait.notify_all();
			cv_FindThread_Wait.wait(lck, [=] {return a_FT_state & mask; });
#if DEBUG_Thr
			db_log(msg[5]);
#endif
			//waked up from ctrl thread
			lck.unlock();
#if DEBUG_Thr
			db_log(msg[3]);
#endif
			if (op == 0xff)
				break;//break to stop the thread
		}
		a_FT_state -= mask;
		cv_CtrlThread_Wait.notify_all();
#if DEBUG_Thr
		db_log(msg[4]);
#endif
		return;
	}

	void FindInBuffer(const int8_t tCount, BufferOP & op, BufferReport drep[], ChkItem & chkdata)
	{
		thread t_find[64];
		uint8_t ftop = 0x0;//op of FindThread
		const uint64_t mask = 0xffffffffffffffffUi64 << tCount;

		unique_lock <mutex> lck_BufUse(mtx_Buf_Use);
		unique_lock <mutex> lck_FindThread(mtx_FindThread_Wait);
		//init
		BufPre(BlkInfo[0]);
		for (int8_t a = 0; a < tCount; a++)
			t_find[a] = thread(FindThread_x64, tCount, a, ref(ftop), &chkdata, ref(drep[a]));
		a_FT_state = 0xffffffffffffffffUi64;

		for (int8_t a = 0; a < tCount; a++)
			t_find[a].detach();

		//wait for init of findthread
		cv_CtrlThread_Wait.wait(lck_FindThread, [=] {return a_FT_state == mask; });
		//FindThread init finish
#if DEBUG_Thr
		wchar_t db_str[120];
		db_log(L"Init ok %d Buf_Fthr.\n");
#endif
		//notify upper thread
		op.op = 0;
		cv_Buf_Ready.notify_all();
#if DEBUG_Thr
		db_log(L"BC0 noti M**\n");
#endif
		//give up the mutex to wake up upper thread
		cv_Buf_Use.wait(lck_BufUse, [&] {return op.op != 0; });
#if DEBUG_Thr
		db_log(L"BC0 wa<- M**\n");
#endif
		//start into the main part
		while (true)
		{
			for (int8_t a = 0; a < tCount; a++)
				drep[a].isFind = 0;
			a_FindLen = 0;
			ftop = 0;
			a_FT_state = 0xffffffffffffffffUi64;
			//wake up all find-thread
			cv_FindThread_Wait.notify_all();
#if DEBUG_Thr
			db_log(L"BC0 noti BTa\n");
#endif
			cv_CtrlThread_Wait.wait(lck_FindThread, [=] {return a_FT_state == mask; });
#if DEBUG_Thr
			db_log(L"BC0 wa<- BTa\n");
#endif
			//waked up from find-thread

			op.op = 0x7f;
			cv_Buf_Ready.notify_all();
#if DEBUG_Thr
			db_log(L"BC0 noti M**\n");
#endif
			cv_Buf_Use.wait(lck_BufUse, [&] {return op.op != 0X7f; });
#if DEBUG_Thr
			db_log(L"BC0 wa<- M**\n");
#endif
			//waked up from upper-thread

			if (op.op == 0xff)//find finish
				break;
			//add buf
			BufAdd(op.len, op.data);
		}
		//end of finding

		ftop = 0xff;
		a_FT_state = 0xffffffffffffffffUi64;
		cv_FindThread_Wait.notify_all();//wake up all find-thread
#if DEBUG_Thr	
		db_log(L"BC0 noti BTa\n");
#endif
		cv_CtrlThread_Wait.wait(lck_FindThread, [=] {return a_FT_state == mask; });
#if DEBUG_Thr
		db_log(L"BC0 noti BTa\n");
#endif
		//all find-thread finish
		op.op = 0x7f;
		cv_Buf_Ready.notify_all();
#if DEBUG_Thr
		db_log(L"BC0 noti M**\n");
#endif
		lck_BufUse.unlock();
		return;
	}

}