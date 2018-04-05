#include "fuz.h"
#include "..\util.h"
#include <chrono>
#include <cassert>
#include <mutex>

DWORD RunPipedProcess(wstring cmdline, string &output, bool force_output)
{
	SECURITY_ATTRIBUTES sa;
	HANDLE stdout_rd = NULL;
	HANDLE stdout_wr = NULL;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;
	CreatePipe(&stdout_rd, &stdout_wr, &sa, 131072);
	SetHandleInformation(stdout_rd, HANDLE_FLAG_INHERIT, 0);

	PROCESS_INFORMATION pi;
	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(STARTUPINFOW));
	si.cb = sizeof(STARTUPINFOW);
	si.dwFlags = STARTF_FORCEOFFFEEDBACK | STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = stdout_wr;
	si.hStdError = stdout_wr;
	if(!CreateProcessW(NULL, &cmdline.front(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
	{
		output = "CreateProcess failed! Error: " + GetLastErrorStr();
		return 0xDeadBeef;
	}
	DWORD res = WaitForSingleObject(pi.hProcess, 10000);
	if(res == WAIT_TIMEOUT)
	{
		output = "Process timed out (10 seconds), terminating.";
		TerminateProcess(pi.hProcess, 1);
		Sleep(500);
		CloseHandle(stdout_rd);
		CloseHandle(stdout_wr);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return 0xDeadBeef;
	}
	else if(res == WAIT_FAILED)
	{
		output = "WaitForSingleObject failed! Error: " + GetLastErrorStr();
		return 0xDeadBeef;
	}

	DWORD exitCode = 1337;
	BOOL b = GetExitCodeProcess(pi.hProcess, &exitCode);
	if(!b) output = "GetExitCodeProcess() failed!\n";
	if(force_output || (b && exitCode != 0))
	{
		string buffer(131072, '\0');
		DWORD chars_read;
		ReadFile(stdout_rd, &buffer.front(), 131071, &chars_read, NULL);
		for(size_t n = 0; n<buffer.size(); n++)
			if(buffer.at(n) == '\r') buffer.replace(n, 2, "\n");
		output = buffer.substr(0, buffer.find('\0'));
	}

	CloseHandle(stdout_rd);
	CloseHandle(stdout_wr);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return exitCode;
}


inline bool is_ascii(const wstring &str)
{
	string test;
	wctomb(str, test);
	for(auto c : test) if(unsigned(c) > 127) return false;
	return true;
}


string fuz_extract(const string &buffer, wstring destdir, string fname, bool cvt, bool lip)
{
	static mutex mtx;
	if(destdir.back() != '\\') destdir.push_back('\\');

	wstring fnamew;
	mbtowc(fname, fnamew);
	wstring xwmfile = destdir + fnamew.substr(0, fnamew.rfind('.')) + L".xwm", 
		wavfile = destdir + fnamew.substr(0, fnamew.rfind('.')) + L".wav";
	wstring cmdline{xenc + L" \"" + xwmfile + L"\" \"" + wavfile + L" \""};
	bool ascii(is_ascii(cmdline));

	ofstream f(xwmfile, ofstream::binary|ofstream::trunc);
	if(!f.good())
	{
		string xwmstr, lasterr{GetLastErrorStr()};
		wctomb(xwmfile, xwmstr);
		return "fuz_extract: failed to open xwm file for writing: " + xwmstr + "\nError: " + lasterr + "\n";
	}
	DWORD32 lipsize(0);
	memcpy(&lipsize, &buffer[8], 4);
	f.write(&buffer[lipsize + 12], buffer.size() - (lipsize + 12)); // *** write .xwm ***
	if(!f.good())
	{
		return "fuz_extract: failed to write xwm file to %temp% directory\nError: " + GetLastErrorStr() + "\n";
	}
	f.close();

	if(lip && lipsize)
	{
		wstring lipfname{fnamew.substr(0, fnamew.rfind('.')) + L".lip"};
		f.open(destdir + lipfname, ofstream::binary|ofstream::trunc);
		f.write(&buffer[12], lipsize);
		f.close();
	}
	if(!cvt) return "";

	DWORD retxwm(0), retwav(0);
	wstring shortxwmpath(xwmfile.size(), '\0'), shortwavpath(wavfile.size(), '\0');

	if(!ascii) // xWMAEncode.exe doesn't like Unicode characters, so feed it short file names
	{
		retxwm = GetShortPathNameW(xwmfile.data(), &shortxwmpath.front(), xwmfile.size());
		CloseHandle(CreateFileW(wavfile.data(), GENERIC_WRITE, FILE_SHARE_DELETE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL));
		retwav = GetShortPathNameW(wavfile.data(), &shortwavpath.front(), wavfile.size());
		DeleteFileW(wavfile.data());
		if(retxwm && retwav)
		{
			shortxwmpath.resize(retxwm);
			shortwavpath.resize(retwav);
			cmdline = xenc + L" \"" + shortxwmpath + L"\" \"" + shortwavpath + L" \"";
		}
	}

	string xenc_out;
	DWORD exit_code = RunPipedProcess(cmdline, xenc_out, false);
	if(xenc_out.empty())
	{
		if(!ascii && retxwm && retwav) MoveFileW(shortwavpath.data(), wavfile.data());
		unsigned tries(0);
		while(!DeleteFileW(xwmfile.data()))
		{
			Sleep(50);
			if(tries++ == 10)
				return "fuz_extract - failed to delete \"" + filepath{xwmfile}.fullname() + "\". Error:" + GetLastErrorStr();
		}
		return "";
	}

	if(exit_code == 0xDeadBeef)
		return "fuz_extract - " + xenc_out;
	return "xWMAEncode.exe: " + xenc_out;
}
