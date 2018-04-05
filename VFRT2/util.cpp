#include "util.h"
#include <sstream> // to_hex_string()

LONGLONG GetFileSize(LPCWSTR fname)
{
	HANDLE hfile = CreateFileW(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	LARGE_INTEGER lint;
	GetFileSizeEx(hfile, &lint);
	CloseHandle(hfile);
	return lint.QuadPart;
}


string GetLastErrorStr()
{
	char err[4096];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, err, 4096, 0);
	string str(err);
	size_t pos = str.find_first_of("\r\n");
	if(pos != string::npos) str.erase(pos);
	return str;
}


wstring GetLastErrorStrW()
{
	wchar_t err[4096];
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, err, 4096, 0);
	wstring str(err);
	size_t pos = str.find_first_of(L"\r\n");
	if(pos != string::npos) str.erase(pos);
	return str;
}


wstring GetAppFolder()
{
	wstring progdirw(4096, '\0');
	progdirw.resize(GetModuleFileNameW(NULL, &progdirw.front(), 4096));
	return progdirw;
}


filepath AppPath()
{
	wstring progdirw(4096, '\0');
	progdirw.resize(GetModuleFileNameW(NULL, &progdirw.front(), 4096));
	return progdirw;
}


wstring MakeTempFolder(wstring folder)
{
	wstring temp_path(1234, '\0');
	temp_path.resize(GetTempPathW(temp_path.size(), &temp_path.front()));
	if(temp_path.empty())
	{
		wstring msg = L"Failed to retrieve the path of the folder designated for temporary files\nError: " + GetLastErrorStrW();
		MessageBoxW(0, msg.data(), L"Error", MB_ICONERROR);
		return L"";
	}
	wstring tp(temp_path+folder);
	if(!FileExist(tp))
	{
		if(CreateDirectoryW(tp.data(), NULL)) temp_path = tp + L'\\';
		else
		{
			wstring msg = L"Failed to create temp folder \"" + tp + L"\"\nError: " + GetLastErrorStrW();
			MessageBoxW(0, msg.data(), L"Error", MB_ICONERROR);
			return L"";
		}
	}
	else temp_path = tp + L'\\';
	return temp_path;
}


string to_hex_string(unsigned i)
{
	stringstream ss;
	ss << hex << i;
	return "0x" + ss.str();
}


filepath::filepath(const string s)
{
	path_ = s;
	mbtowc(s, pathw_);
	unsigned pos = s.rfind('\\');
	dir_ = s.substr(0, pos);
	mbtowc(dir_, dirw_);
	unsigned pos2 = s.rfind('.');
	name_ = s.substr(pos+1, pos2-pos-1);
	mbtowc(name_, namew_);
	ext_ = s.substr(pos2+1);
	mbtowc(ext_, extw_);
	fullname_ = s.substr(pos+1);
	mbtowc(fullname_, fullnamew_);
}

filepath::filepath(const wstring s)
{
	pathw_ = s;
	wctomb(s, path_);
	unsigned pos = s.rfind(L'\\');
	dirw_ = s.substr(0, pos);
	wctomb(dirw_, dir_);
	unsigned pos2 = s.rfind(L'.');
	namew_ = s.substr(pos+1, pos2-pos-1);
	wctomb(namew_, name_);
	extw_ = s.substr(pos2+1);
	wctomb(extw_, ext_);
	fullnamew_ = s.substr(pos+1);
	wctomb(fullnamew_, fullname_);
}


bool CopyToClipboard(const wstring &str, HWND hwnd)
{
	if(!OpenClipboard(hwnd)) return false;
	if(!EmptyClipboard()) return false;
	size_t buflen = (str.size()+1)*2;
	HGLOBAL hdata = GlobalAlloc(GMEM_MOVEABLE, buflen);
	LPWSTR lpwstr = (LPWSTR)GlobalLock(hdata);
	memcpy_s(lpwstr, buflen-2, &str.front(), buflen-2);
	lpwstr[str.size()] = L'\0';
	GlobalUnlock(hdata);
	auto res = SetClipboardData(CF_UNICODETEXT, hdata);
	CloseClipboard();
	return res != NULL;
}