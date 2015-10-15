#pragma once

namespace acp
{
	extern uint8_t *for_t_data;
	struct ChkItem;
	struct BufferReport
	{
		uint8_t isFind;
		uint8_t *addr;
		uint8_t objlen;
		uint32_t offset;
		uint8_t *p_b;
		uint32_t s_b;
	};

	struct BufferOP
	{
		//0xfe:add buffer
		uint8_t op;//operation code
		uint8_t len;//data len for add buf
		uint8_t data[300];//data for add buf
	};

	extern mutex mtx_Buf_Use;
	extern condition_variable cv_Buf_Use,
		cv_Buf_Ready;

	void Buffer_init(uint16_t bufcount);
	void Buffer_exit();
	void BufAdd(uint16_t len, uint8_t data[]);
	uint8_t BufGet(uint32_t offset, uint8_t len, uint8_t data[]);
	void FindInBuffer(const int8_t tCount, BufferOP &op, BufferReport drep[], ChkItem &chkdata);
	//void dumpbuffer();
}
