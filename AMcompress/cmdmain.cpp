#include "rely.h"
#include "basic.h"
#include "compress.h"
#include "uncompress.h"

#define VER 1510171443Ul

using namespace acp;

enum class cl :int8_t { comp, uncomp, debug, fname, bufsize, dicsize, wrong, thread, vtune };
vector<ParseTable> ptab
{
	{ L"",(int8_t)cl::wrong,'n' },
	{ L"c",(int8_t)cl::comp,'n' },
	{ L"u",(int8_t)cl::uncomp,'n' },
	{ L"debug",(int8_t)cl::debug,'n' },
	{ L"f",(int8_t)cl::fname,'s' },
	{ L"thread",(int8_t)cl::thread,'i' },
	{ L"buf",(int8_t)cl::bufsize,'i' },
	{ L"dic",(int8_t)cl::dicsize,'i' },
	{ L"vtune",(int8_t)cl::vtune,'n' },
};


void ShowProgress(bool &isRun, ProgressInfo &pinfo, bool cORu)
{
	if (cORu)//compress
	{
		wprintf(L"========== Start Compress ==========\n");
		while (isRun)
		{
			if (pinfo.inlen)
			{
				wprintf(L"Compress %lld / %lld , rate %f%%\r", pinfo.innow, pinfo.inlen, pinfo.outnow*100.0 / pinfo.innow);
			}
			this_thread::sleep_for(chrono::milliseconds(50));
		}
	}
	else//uncompress
	{
		wprintf(L"========= Start Uncompress =========\n");
		while (isRun)
		{
			if (pinfo.inlen)
			{
				wprintf(L"Uncompress %lld / %lld , finish %f%%\r", pinfo.outnow, pinfo.outlen, pinfo.outnow*100.0 / pinfo.outlen);
			}
			this_thread::sleep_for(chrono::milliseconds(50));
		}
	}
	return;
}


int wmain(int argc, wchar_t *argv[])
{
	wstring filename;
	int8_t isCU = 0;
	cmder setting;
	cmdpaser p(ptab);
	std::locale::global(std::locale(""));

	wprintf(L"===== Advanced Compress Program =====\n");
	wprintf(L"***** VER %ld *****\n",VER);
#if DEBUG
	wprintf(L"***** DEBUG MODE *****\n");
#endif
	if (argc == 1)
	{
		setting.dictcount = 15000;
		setting.bufcount = 1024;
		setting.thread = 1;
		setting.debug = false;
		wchar_t* args[128];
		int8_t argn = 4;
		args[0] = argv[0];
		
		bool flag = true;
		while (flag)
		{
			int num;
			wprintf(L"1.Compress \t 2.Uncompress \t 0.exit\n");
			wscanf(L"%d", &num);
			switch (num)
			{
			case 0:
				return 0;
			case 1:
				args[1] = L"-c";
				flag = false;
				break;
			case 2:
				args[1] = L"-u";
				//lstrcpyW(args[argn++], L"-u");
				flag = false;
				break;
			}
		}

		args[2] = L"-f";
		wchar_t fn[256];
		wprintf(L"Input File Name:\n");
		wscanf(L"%ls", fn);
		args[3] = fn;

		wprintf(L"Advanced Parameter(finish with CTRL+Z)\n");
		wchar_t *argtmp = new wchar_t[256];
		while (wscanf(L"%ls", argtmp) != EOF)
		{
			args[argn++] = argtmp;
			argtmp = new wchar_t[256];
		}
		delete[] argtmp;

		wprintf(L"Args View:\n");
		for (auto a = 0; a < argn; a++)
			wprintf(L"arg %d :%ls\n", a, args[a]);

		p.init(argn, args);

		for (auto a = argn; a > 4;)
			delete[] args[--a];
	}
	else
	{
		/*
		wprintf(L"Args View:\n");
		for (auto a = 0; a < argc; a++)
			wprintf(L"arg %d :%ls\n", a, argv[a]);
		*/
		p.init(argc, argv);
	}

	bool isWait = true;

	//parse command
	for (auto a = 0; a < p.size(); a++)
	{
		switch (p.com(a))
		{
		case (int8_t)cl::comp:
			isCU = 1;
			wprintf(L"cmd %d :COMPRESS\n", a);
			break;
		case (int8_t)cl::uncomp:
			isCU = 2;
			wprintf(L"cmd %d :UNCOMPRESS\n", a);
			break;
		case (int8_t)cl::fname:
			filename = p.dataS(a);
			wprintf(L"cmd %d :FNAME %s\n", a, filename.c_str());
			break;
		case (int8_t)cl::debug:
			setting.debug = true;
			wprintf(L"cmd %d :DEBUG\n", a);
			break;
		case (int8_t)cl::thread:
			setting.thread = p.dataI(a);
			wprintf(L"cmd %d :THREAD %d\n", a, p.dataI(a));
			break;
		case (int8_t)cl::bufsize:
			setting.bufcount = p.dataI(a);
			wprintf(L"cmd %d :BUFFER BLK %d\n", a, p.dataI(a));
			break;
		case (int8_t)cl::dicsize:
			setting.dictcount = p.dataI(a);
			wprintf(L"cmd %d :DICT NUM %d\n", a, p.dataI(a));
			break;
		case (int8_t)cl::vtune:
			wprintf(L"cmd %d :VTUNE\n", a);
			isWait = false;
			break;
		case (int8_t)cl::wrong:
			wprintf(L"Error command : %ls\n", p.dataS(a).c_str());
			break;
		default:
			break;
		}
	}

	if (isCU == 0)
	{
		wprintf(L"no command for deciding runing mode\n");
		system("pause");
		return 0;
	}


	LARGE_INTEGER t_s, t_e, t_f;
	double t;
	QueryPerformanceFrequency(&t_f);

	ProgressInfo pinfo;
	bool isShowProgress = true;
	thread T_sp;

	if (isCU == 1)//compress
	{
		T_sp = thread(ShowProgress, ref(isShowProgress), ref(pinfo), true);
		T_sp.detach();

		QueryPerformanceCounter(&t_s);
		auto ret = compress(filename, setting, pinfo);
		QueryPerformanceCounter(&t_e);

		isShowProgress = false;
		wprintf(L"Compress %lld / %lld , rate %f%%\n", pinfo.innow, pinfo.inlen, pinfo.outnow*100.0 / pinfo.innow);
		wprintf(L"============== Finish ==============\n");

		switch (ret)
		{
		case 0x0:
			t = (t_e.QuadPart - t_s.QuadPart)*1.0 / t_f.QuadPart;
			wprintf(L"Finish.\nTime cost: %f s\n", t);
			wprintf(L"input  file:%12lld bytes\noutput file:%12lld bytes\n", pinfo.inlen, pinfo.outlen);
			wprintf(L"======compress rate=%f%%\n", pinfo.outlen*100.0 / pinfo.inlen);
			break;
		case 0x1:
			wprintf(L"Cannot open input file.\nExit\n");
			break;
		case 0x2:
			wprintf(L"Cannot open output file.\nExit\n");
			break;
		default:
			wprintf(L"Error happens during compress.\nExit\n");
		}
	}
	else if (isCU == 2)//uncompress
	{
		if (filename.find(L".amc") == string::npos)
			filename.append(L".amc");

		T_sp = thread(ShowProgress, ref(isShowProgress), ref(pinfo), false);
		T_sp.detach();

		QueryPerformanceCounter(&t_s);
		auto ret = uncompress(filename, setting, pinfo);
		QueryPerformanceCounter(&t_e);

		isShowProgress = false;
		wprintf(L"Uncompress %lld / %lld , finish %f%%\n", pinfo.outnow, pinfo.outlen, pinfo.outnow*100.0 / pinfo.outlen);
		wprintf(L"============== Finish ==============\n");

		switch (ret)
		{
		case 0x0:
			t = (t_e.QuadPart - t_s.QuadPart)*1.0 / t_f.QuadPart;
			wprintf(L"Finish.\nTime cost: %f s\n", t);
			wprintf(L"output file:%12lld bytes\ninput  file:%12lld bytes\n", pinfo.outlen, pinfo.inlen);
			wprintf(L"======compress rate=%f%%\n", pinfo.inlen*100.0 / pinfo.outlen);
			break;
		case 0x1:
			wprintf(L"Cannot open input file.\nExit\n");
			break;
		case 0x2:
			wprintf(L"Cannot open output file.\nExit\n");
			break;
		default:
			wprintf(L"Error happens during uncompress.\nExit\n");
		}
	}
	
	if(isWait)
		system("pause");
	return 0;
	
}