#include "rely.h"
#include "basic.h"
#include "dict.h"
#include "buffer.h"
#include "coder.h"
#include "bitfile.h"

namespace acp
{
	mutex mtx_Coder_Use;
	condition_variable cv_Coder_Use,
		cv_Coder_Ready;

	static uint16_t ob_len = 0;//len of output buffer
	static uint8_t out_buffer[300];//output data

	CodeAns Code_TestDict(int8_t num, DictReport drep[])
	{
		_mm_prefetch((char*)drep, _MM_HINT_T0);
	#if DEBUG_thr 
		db_log(L"CTD start\n");
	#endif
		CodeAns ret, tmpret;
		uint8_t tmplen;//for calc len(bit) that cost
		ret.savelen = (int16_t)0x8000;
		ret.srclen = 0;
		for (auto a = 0; a < num; ++a)
		{
			if (drep[a].isFind == 0)//not find
				continue;
			//judge dictID
			tmpret.dID = drep[a].dicID;
			tmpret.srclen = drep[a].objlen;
			tmpret.addr = drep[a].addr;
			if (drep[a].dicID < 64)
			{//freq dict
				tmpret.type = 0x10;
				tmplen = 9;
				tmpret.part_len[0] = 2;
				tmpret.part_data[0] = 0b11;
				tmpret.part_len[1] = 6;
				tmpret.part_data[1] = drep[a].dicID;
			}
			else if(drep[a].dicID < 1088)
			{//norm dict
				tmpret.type = 0x20;
				tmplen = 15;
				tmpret.part_len[0] = 4;
				tmpret.part_data[0] = 0b0011;
				tmpret.part_len[1] = 10;
				tmpret.part_data[1] = drep[a].dicID - 64;
			}
			else
			{//rare dict
				tmpret.type = 0x30;
				tmplen = 19;
				tmpret.part_len[0] = 4;
				tmpret.part_data[0] = 0b0001;
				tmpret.part_len[1] = 14;
				tmpret.part_data[1] = drep[a].dicID - 1088;
			}

			//judge align
			if (drep[a].diclen == drep[a].objlen)
			{//full
				tmpret.type += 0x01;
				tmpret.part_num = 3;
				tmpret.part_len[2] = 1;
				tmpret.part_data[2] = 0b1;
			}
			else
			{
				uint8_t tmpa, tmpb, right_dis = drep[a].diclen - drep[a].offset - drep[a].objlen;
				tmpret.part_len[2] = 2;
				if (drep[a].offset < right_dis)
				{
					tmpa = drep[a].offset;
					tmpret.part_data[2] = 0b01;
					tmpb = right_dis - drep[a].offset;
				}
				else
				{
					tmpa = right_dis;
					tmpret.part_data[2] = 0b00;
					tmpb = drep[a].offset - right_dis;
				}
				//if (right_dis < 0)
					//printf("\n\n\n");
				if (drep[a].diclen > 48)
				{
					if (tmpa < 16 && tmpb < 16)
					{//mid
						tmpret.type += 0x02;
						tmplen += 9;
						tmpret.part_num = 5;
						tmpret.part_len[3] = 4;
						tmpret.part_data[3] = tmpa;
						tmpret.part_len[4] = 4;
						tmpret.part_data[4] = tmpb;
					}
					else
						continue;//uncodable
				}
				else if (drep[a].diclen > 32)
				{
					if (tmpa < 8 && tmpb < 16)
					{//mid
						tmpret.type += 0x02;
						tmplen += 8;
						tmpret.part_num = 5;
						tmpret.part_len[3] = 3;
						tmpret.part_data[3] = tmpa;
						tmpret.part_len[4] = 4;
						tmpret.part_data[4] = tmpb;
					}
					else
						continue;//uncodable
				}
				else if (drep[a].diclen > 16)
				{
					if (tmpa < 8 && tmpb < 8)
					{//mid
						tmpret.type += 0x02;
						tmplen += 7;
						tmpret.part_num = 5;
						tmpret.part_len[3] = 3;
						tmpret.part_data[3] = tmpa;
						tmpret.part_len[4] = 3;
						tmpret.part_data[4] = tmpb;
					}
					else
						continue;//uncodable
				}
				else if (drep[a].diclen > 8)
				{
					if (tmpa < 4 && tmpb < 8)
					{//mid
						tmpret.type += 0x02;
						tmplen += 5;
						tmpret.part_num = 5;
						tmpret.part_len[3] = 2;
						tmpret.part_data[3] = tmpa;
						tmpret.part_len[4] = 3;
						tmpret.part_data[4] = tmpb;
					}
					else
						continue;//uncodable
				}
				else
				{
					if (tmpa < 2 && tmpb < 4)
					{//mid
						tmpret.type += 0x02;
						tmplen += 4;
						tmpret.part_num = 5;
						tmpret.part_len[3] = 1;
						tmpret.part_data[3] = tmpa;
						tmpret.part_len[4] = 2;
						tmpret.part_data[4] = tmpb;
					}
					else
						continue;//uncodable
				}
			}

			//judge update
			tmpret.savelen = (int16_t)tmpret.srclen * 8 - tmplen;
			if (ret.savelen < tmpret.savelen)
				ret = tmpret;
		}
	#if DEBUG_Thr
		db_log(L"CTD finish\n");
	#endif
		//ret.savelen = (int16_t)0x8000;
		//ret.srclen = 0;
		return ret;
	}

	CodeAns Code_TestBuffer(int8_t num, BufferReport brep[])
	{
	#if DEBUG_Thr
		db_log(L"CTB start\n");
	#endif
		CodeAns ret, tmpret;
		uint8_t tmplen;//for calc len(bit) that cost
		ret.savelen = (int16_t)0x8000;
		ret.srclen = 0;
		for (auto a = 0; a < num; ++a)
		{
			if (brep[a].isFind == 0)//not find
				continue;
			tmpret.part_data[5] = brep[a].offset;
			//tmpret.p_b = brep[a].p_b;
			//tmpret.s_b = brep[a].s_b;
			tmpret.part_num = 4;
			tmpret.srclen = brep[a].objlen;
			//judge offset
			tmpret.addr = brep[a].addr;
			if (brep[a].offset < 3)
				continue;
			if (brep[a].offset < 515)
			{//near
				tmpret.type = 0x1;
				tmplen = 15;
				tmpret.part_len[0] = 2;
				tmpret.part_data[0] = 0b10;
				tmpret.part_len[1] = 9;
				tmpret.part_data[1] = brep[a].offset - 3;
			}
			else
			{
				if (brep[a].offset < 16899)
				{//norm
					tmpret.type = 0x2;
					tmplen = 21;
					tmpret.part_len[0] = 3;
					tmpret.part_data[0] = 0b011;
					tmpret.part_len[1] = 14;
					tmpret.part_data[1] = brep[a].offset - 515;
				}
				else
				{//far
					tmpret.type = 0x3;
					tmplen = 30;
					tmpret.part_len[0] = 4;
					tmpret.part_data[0] = 0b0010;
					tmpret.part_len[1] = 22;
					tmpret.part_data[1] = brep[a].offset - 16899;
				}
			}

			//put len
			if (brep[a].objlen < 11)
			{//short
				tmpret.part_len[2] = 1;
				tmpret.part_data[2] = 0b1;
				tmpret.part_len[3] = 3;
				tmpret.part_data[3] = brep[a].objlen - 3;
			}
			else
			{//long
				tmplen += 3;
				tmpret.part_len[2] = 1;
				tmpret.part_data[2] = 0b0;
				tmpret.part_len[3] = 6;
				tmpret.part_data[3] = brep[a].objlen - 3;
			}
			//judge update
			tmpret.savelen = (int16_t)tmpret.srclen * 8 - tmplen;
			if (ret.savelen < tmpret.savelen)
				ret = tmpret;
		}
	#if DEBUG_Thr
		db_log(L"CTB finish\n");
	#endif
		return ret;
	}

	static inline void putRAW(bitWfile &out)
	{
		_mm_prefetch((char*)out_buffer, _MM_HINT_NTA);
	#if DEBUG
		wchar_t db_str[120];
	#endif
	#if DEBUG_Thr
		swprintf(db_str, L"^^output RAW $%d\n", ob_len);
		db_log(db_str);
	#endif
		if (ob_len < 9)//short RAW
		{
		#if DEBUG_Com
			swprintf(db_str, L"RAW short len=%d\n", ob_len);
			db_com(db_str, false);
		#endif
			out.putBits(3, 0b010);
			out.putBits(3, ob_len - 1);
		}
		else if (ob_len < 41)//normal RAW
		{
		#if DEBUG_Com
			swprintf(db_str, L"RAW normal len=%d\n", ob_len);
			db_com(db_str, false);
		#endif
			out.putBits(5, 0b00001);
			out.putBits(5, ob_len - 9);
			//out.putBits(7, 0x0);
		}
		else//long RAW
		{
		#if DEBUG_Com
			swprintf(db_str, L"RAW long len=%d\n", ob_len);
			db_com(db_str, false);
		#endif
			_mm_prefetch((char*)out_buffer + 64, _MM_HINT_NTA);
			_mm_prefetch((char*)out_buffer + 128, _MM_HINT_NTA);
			out.putBits(6, 0b000001);
			out.putBits(8, ob_len - 41);
		}
		out.putChars(ob_len, out_buffer);
		ob_len = 0;
		return;
	}

	static inline void showCA(CodeAns &CA, wchar_t *str)
	{
		if (CA.type & 0xf0)
		{
			wchar_t *tdat,
				adat[256];
			bool altype;
			uint16_t dID = CA.part_data[1];
			//Dict
			switch (CA.type & 0xf0)
			{
			case 0x10:tdat = L"freq"; break;
			case 0x20:tdat = L"norm"; dID += 64; break;
			case 0x30:tdat = L"rare"; dID += 1088; break;
			default:tdat = L"err "; break;
			}
			switch (CA.type & 0xf)
			{
			case 0x1:swprintf(adat, L"full"); break;
			case 0x2:
				altype = !!CA.part_data[2];
				uint8_t tmpa, tmpb;
				tmpa = CA.part_data[3];
				tmpb = CA.part_data[4] + tmpa;
				swprintf(adat, L"mid  l=%d r=%d", altype?tmpa:tmpb, altype?tmpb:tmpa); break;
			default:wcscat(adat, L"err "); break;
			}
			swprintf(str, L"DIC %s id=%d %s\n", tdat, dID, adat);
			return;
		}
		else
		{
			//Buffer
			switch (CA.type & 0xf)
			{
			case 0x1:swprintf(str, L"BUF near off=%d len=%d\n", CA.part_data[1] + 3, CA.part_data[3] + 3); break;
			case 0x2:swprintf(str, L"BUF norm off=%d len=%d\n", CA.part_data[1] + 515, CA.part_data[3] + 3); break;
			case 0x3:swprintf(str, L"BUF far  off=%d len=%d\n", CA.part_data[1] + 16899L, CA.part_data[3] + 3); break;
			default:swprintf(str, L"BUF err \n"); break;
			}
			return;
		}
	}

	static inline void putCA(bitWfile &out, CodeAns &CA)
	{
	#if DEBUG
		wchar_t db_str[120];
	#endif
	#if DEBUG_Thr
		swprintf(db_str, L"^^output CA $%d\n", ob_len);
		db_log(db_str);
	#endif
		for (auto a = 0; a < CA.part_num; ++a)
		{
			out.putBits(CA.part_len[a], CA.part_data[a]);
		}
	#if DEBUG_Com
		showCA(CA, db_str);
		uint64_t ret = db_com(db_str, false);
	#endif
		return;
	}

	static inline void putEnd(bitWfile &out)
	{
		out.putBits(7, 0x1);
	#if DEBUG
		db_com(L"END\n", false);
	#endif
		return;
	}

	void CoderT(bitWfile &out, CoderOP &op)
	{
		unique_lock <mutex> lck_CoderUse(mtx_Coder_Use);
		//init

		//notify upper thread
		op.op = 0;

		cv_Coder_Ready.notify_all();
	#if DEBUG_Thr
		db_log(L"Coder_thr notify.\n");
	#endif

		while (true)
		{
			//give up the mutex to wake up upper thread
			cv_Coder_Use.wait(lck_CoderUse, [&] { return op.op != 0; });
			//waked up from upper-thread
				//#if DEBUG { swprintf(db_str, L"Coder_thr wake.\n"); db_log(db_str);}

			if (op.op == 0xfe)
			{
				//put byte
				if (ob_len > 295)
					putRAW(out);
				out_buffer[ob_len++] = op.bdata;
			}
			else if (op.op == 0xfd)
			{
				//put CodeAns
				if (ob_len)
					//out put raw data first
					putRAW(out);
				putCA(out, op.cdata);
			}
			else if (op.op == 0xff)
			{
				//finish
				if (op.enddata[0])//still little data
					for (auto a = 0; a < op.enddata[0];)
					{
						if (ob_len > 295)
							putRAW(out);
						out_buffer[ob_len++] = op.enddata[++a];
					}
				if (ob_len)
					//out put raw data first
					putRAW(out);
				putEnd(out);
				break;
			}
			op.op = 0;
			cv_Coder_Ready.notify_all();
		}
		op.op = 0x7f;
		cv_Coder_Ready.notify_all();
		lck_CoderUse.unlock();
		return;
	}

	void Coder(bitWfile &out, CoderOP &op)
	{
		switch (op.op)
		{
		case 0xfe://put byte
			if (ob_len > 295)
				putRAW(out);
			out_buffer[ob_len++] = op.bdata;
			break;
		case 0xfd://put CodeAns
			if (ob_len)
				//out put raw data first
				putRAW(out);
			putCA(out, op.cdata);
			break;
		case 0xff://finish
			if (op.enddata[0])//still little data
				for (auto a = 0; a < op.enddata[0];)
				{
					if (ob_len > 295)
						putRAW(out);
					out_buffer[ob_len++] = op.enddata[++a];
				}
			if (ob_len)
				//out put raw data first
				putRAW(out);
			putEnd(out);
			break;
		}
		return;
	}

	bool DeCoder(bitRfile &in, DecoderOP &op)
	{
	#if DEBUG
		wchar_t db_str[120];
	#endif
		wchar_t *type = L"empty";
		if (in.eof())//end
		{
			in.eof();
			return false;
		}

		if (in.getNext())
		{//1@
			if (in.getNext())
			{
				//freq dict
				op.op = 0x2;
				type = L"freq";
				op.dID = in.getBits(6);
			}
			else
			{
				//near buffer
				op.op = 0x3;
				type = L"near";
				op.offset = in.getBits(9) + 3;
			}
		}
		else
		{//1#
			if (in.getNext())
			{//2@
				if (in.getNext())
				{
					//norm buffer
					op.op = 0x3;
					type = L"norm";
					op.offset = in.getBits(14) + 515;
				}
				else
				{
					//short RAW
					op.op = 0x1;
					op.len = in.getBits(3) + 1;
					type = L"short";
				}
			}
			else
			{//2#
				if (in.getNext())
				{//3@
					//norm dict and far buffer
					if (in.getNext())
					{
						//norm dict
						op.op = 0x2;
						type = L"norm";
						op.dID = in.getBits(10) + 64;
					}
					else
					{
						//far buffer
						op.op = 0x3;
						type = L"far ";
						op.offset = in.getBits(22) + 16899;
					}
				}
				else
				{//3#
					if (in.getNext())
					{
						//rare dict
						op.op = 0x2;
						type = L"rare";
						op.dID = in.getBits(14) + 1088;
					}
					else
					{//4#
						//RAW or command
						if (in.getNext())
						{
							//normal RAW
							op.op = 0x1;
							op.len = in.getBits(5) + 9;
							type = L"normal";
						}
						else
						{//5#
							//long RAW or command
							if (in.getNext())
							{
								//long RAW
								op.op = 0x1;
								op.len = in.getBits(8) + 41;
								type = L"normal";
							}
							else
							{//6#
								//command
								if (in.getNext())
								{
									//end
								#if DEBUG
									db_com(L"END\n", true);
								#endif
									op.op = 0xff;
									return false;
								}
								else
								{
									//unknown
									op.op = 0xf0;
								}
								//end of command
							}
							//end of long RAW or command
						}
						//end of RAW or command
					}
					//end of uncommon com
				}
				//end of not near buffer
			}
			//end of not freq dict
		}
		//end of reading type
		switch (op.op)
		{
		case 0x1://RAW
			in.getChars(op.len, op.data);
		#if DEBUG
			swprintf(db_str, L"RAW %s len=%d\n", type, op.len);
		#endif
			break;
		case 0x2://dict
			//judge align
		{
			if (in.getNext())
			{
				//full dict
				op.len = 0xff;
				op.offset = 0x0;
			#if DEBUG
				swprintf(db_str, L"DIC %s id=%d full\n", type, op.dID);
			#endif
			}
			else
			{
				bool altype = in.getNext();
				uint8_t tmpa, tmpb;
				uint8_t dlen = getDictLen(op.dID);
				if (dlen > 48)
				{
					tmpa = in.getBits(4);
					tmpb = in.getBits(4) + tmpa;
				}
				else if (dlen > 32)
				{
					tmpa = in.getBits(3);
					tmpb = in.getBits(4) + tmpa;
				}
				else if (dlen > 16)
				{
					tmpa = in.getBits(3);
					tmpb = in.getBits(3) + tmpa;
				}
				else if (dlen > 8)
				{
					tmpa = in.getBits(2);
					tmpb = in.getBits(3) + tmpa;
				}
				else
				{
					tmpa = in.getBits(1);
					tmpb = in.getBits(2) + tmpa;
				}

				//tmpa = in.getBits(4);
				//tmpb = in.getBits(4) + tmpa;
				if (altype)
				{
					op.offset = tmpa;
					op.len = 0xff - tmpb;
				}
				else
				{
					op.offset = tmpb;
					op.len = 0xff - tmpa;
				}
			#if DEBUG
				swprintf(db_str, L"DIC %s id=%d mid  l=%d r=%d\n", type, op.dID, op.offset, 0xff - op.len);
			#endif
			}
		}
		break;
		case 0x3://buffer
		{
			if (in.getNext())
			{
				//short buffer
				op.len = in.getBits(3) + 3;
			}
			else
			{
				//long buffer
				op.len = in.getBits(6) + 3;
			}
		#if DEBUG
			swprintf(db_str, L"BUF %s off=%d len=%d\n", type, op.offset, op.len);
		#endif
		}
		break;
		default:
			break;
		}
	#if DEBUG
		db_com(db_str, true);
	#endif

		return true;
	}


}