#pragma once

namespace acp
{
	struct ChkItem;
	union DictItem
	{
		struct
		{
			uint8_t data[64];
			uint8_t jump[64];
		}L;
		struct
		{
			uint8_t data[32];
			uint8_t jump[32];
		}S;
	}; 

	struct DictReport
	{
		uint16_t dicID;//dict num
		uint8_t isFind,//is find
			diclen,//dict len
			offset,//dict start pos
			objlen;//object data len
		uint8_t *addr;//addr of the dict
	};

	struct DictOP
	{
		//0xfe:add dict;0xfd:use dict
		uint8_t op;//operation code
		uint8_t findlen;//return find max len
		uint8_t len;//data len for add dict
		uint16_t dID;//dict ID for use dict
		uint32_t bOffset;//offset of buffer
		uint32_t s_b;
		uint8_t *p_b;
		uint8_t data[64];//data for add dict
	};

	extern mutex mtx_Dict_Use;
	extern condition_variable cv_Dict_Use,
		cv_Dict_Ready;

	void Dict_init(uint16_t diccount);
	void Dict_exit();
	void DictAdd(const uint8_t len, const uint8_t data[]);
	uint8_t DictGet(const uint16_t dID, uint8_t offset, uint8_t &len, uint8_t data[]);
	void FindInDict(const int8_t tCount, DictOP &op, DictReport drep[], ChkItem &chkdata);
	void dumpdict();
	uint8_t getDictLen(uint16_t dID);
}
