#pragma once

#include <Windows.h>
#include <string>
#include <chrono>
#include <experimental/filesystem>
#include <Shobjidl.h>
#include <comdef.h>
#include <iostream>
#include <mutex>
#include <assert.h>

#pragma warning( disable : 4800 4267 4996)

using string = std::string;
using wstring = std::wstring;
using namespace std::string_literals;

class filepath
{
	wstring pathw_, dirw_, namew_, extw_, fullnamew_;
	string path_, dir_, name_, ext_, fullname_;

public:
	filepath() {}
	filepath(string);
	filepath(wstring);
	operator string() const noexcept { return path_; }
	operator wstring() const noexcept { return pathw_; }
	string dir() const noexcept { return dir_; }
	wstring dirw() const noexcept { return dirw_; }
	string name() const noexcept { return name_; }
	wstring namew() const noexcept { return namew_; }
	string ext() const noexcept { return ext_; }
	wstring extw() const noexcept { return extw_; }
	string fullname() const noexcept { return fullname_; }
	wstring fullnamew() const noexcept { return fullnamew_; }
	const string &path() const noexcept { return path_; }
	const wstring &wpath() const noexcept { return pathw_; }

	bool empty() { return pathw_.empty(); }

	void clear()
	{ 
		pathw_.clear(); dirw_.clear(); namew_.clear(); extw_.clear(); fullnamew_.clear();
		path_.clear(); dir_.clear(); name_.clear(); ext_.clear(); fullname_.clear();
	}
};


// UTF8 conversion of wide character string (std::wstring) to multibyte string (std::string)
static void wctomb(const wstring &wcstr, string &mbstr, unsigned cp = CP_UTF8)
{
	if(wcstr.empty()) return;
	int len = WideCharToMultiByte(cp, NULL, wcstr.data(), -1, nullptr, 0, NULL, NULL);
	mbstr.assign(len-1, '\0');
	WideCharToMultiByte(cp, NULL, wcstr.data(), -1, &mbstr.front(), len-1, NULL, NULL);
}

// UTF8 conversion of multibyte string (std::string) to wide character string (std::wstring)
static void mbtowc(const string &mbstr, wstring &wcstr, unsigned cp = CP_UTF8)
{
	if(mbstr.empty()) return;
	int len = MultiByteToWideChar(cp, NULL, mbstr.data(), -1, nullptr, 0);
	wcstr.assign(len-1, '\0');
	MultiByteToWideChar(cp, NULL, mbstr.data(), -1, &wcstr.front(), len-1);
}

static string strlower(string s) noexcept { for(auto &c : s) c = tolower(c); return move(s); }
static wstring strlower(wstring s) noexcept { for(auto &c : s) c = tolower(c); return move(s); }

LONGLONG GetFileSize(LPCWSTR);
string GetLastErrorStr();
wstring GetLastErrorStrW();
string to_hex_string(unsigned);
// NO trailing backslash
wstring GetAppFolder();
filepath AppPath();
bool CopyToClipboard(const wstring&, HWND);

// returns path with trailing backslash
wstring MakeTempFolder(wstring = L"VFRT2"); 

template<typename T> bool FileExist(const T &fname) { return std::experimental::filesystem::exists(fname); }

static DWORD NumberOfProcessors()
{
	SYSTEM_INFO sysinfo = {0};
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
}


class chronometer
{
	long long start, elapsed_;
	bool stopped;
	struct time { long long h, m, s; };

public:
	chronometer() { reset(); }
	void stop() { stopped = true; elapsed_ = (std::chrono::system_clock::now().time_since_epoch().count()-start)/10000; }
	void reset() { stopped = false; elapsed_ = 0; start = std::chrono::system_clock::now().time_since_epoch().count(); }

	long long elapsed_ms()
	{ 
		if(stopped) return elapsed_;
		else return (std::chrono::system_clock::now().time_since_epoch().count()-start)/10000;
	}

	long long elapsed_s() { return elapsed_ms()/1000; }

	time elapsed()
	{
		auto es = elapsed_s();
		return { es/3600, (es%3600)/60, (es%3600)%60 };
	}
};


class folder_picker
{
	wstring initial_folder_, picked_folder_;

public:

	struct { HRESULT hres;  wstring text; } error;
	folder_picker() = default;
	folder_picker(wstring init_path) { initial_folder_ = init_path; }
	
	void initial_folder(wstring init_path) { initial_folder_ = init_path; }
	wstring initial_folder() { return initial_folder_; }

	bool show(HWND parent = nullptr)
	{
		bool succeeded = false;
		error.text.clear();
		error.hres = 0;
		HRESULT coinit_res = CoInitialize(NULL);
		error.text = _com_error(coinit_res).ErrorMessage();
		error.hres = coinit_res;
		if(!SUCCEEDED(coinit_res)) return false;
		
		IFileDialog *fd(nullptr);
		HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fd));
		error.text = _com_error(hr).ErrorMessage();
		if(SUCCEEDED(hr))
		{
			fd->SetOptions(FOS_PICKFOLDERS);
			IShellItem *initfolder(nullptr);
			if(initial_folder_.size())
			{
				auto res = SHCreateItemFromParsingName(initial_folder_.data(), nullptr, IID_IShellItem, (void**)&initfolder);
				error.text = _com_error(res).ErrorMessage();
				error.hres = coinit_res;
			}
			if(initfolder) fd->SetFolder(initfolder);
			hr = fd->Show(parent);
			error.text = _com_error(hr).ErrorMessage();
			error.hres = coinit_res;
			if(!SUCCEEDED(hr)) return false; // HRESULT_FROM_WIN32(ERROR_CANCELLED)
			IShellItem *si;
			hr = fd->GetResult(&si);
			error.text = _com_error(hr).ErrorMessage();
			error.hres = coinit_res;
			if(SUCCEEDED(hr))
			{
				PWSTR pwstr(nullptr);
				hr = si->GetDisplayName(SIGDN_FILESYSPATH, &pwstr);
				error.text = _com_error(hr).ErrorMessage();
				error.hres = coinit_res;
				if(SUCCEEDED(hr))
				{
					picked_folder_ = wstring(pwstr);
					succeeded = true;
					CoTaskMemFree(pwstr);
				}
				si->Release();
			}
			fd->Release();
		}
		if(coinit_res == S_OK) CoUninitialize();
		return succeeded;
	}

	auto picked_folder() { return picked_folder_; }
};


class regkey
{
	HKEY key{nullptr};
	LSTATUS res{0};

public:

	regkey(HKEY hkey, const string &subkey)
	{
		res = RegOpenKeyExA(hkey, subkey.data(), 0, KEY_QUERY_VALUE, &key);
	}

	wstring get_string(const wstring &value_name)
	{
		if(res != ERROR_SUCCESS) return L"";
		DWORD bufsize(8192); // buffer size in bytes
		wstring strval(bufsize/2, '\0');
		res = RegQueryValueExW(key, value_name.data(), 0, 0, (LPBYTE)&strval.front(), &bufsize);
		if(res == ERROR_SUCCESS)
		{
			auto pathsize = bufsize/2;
			if(strval[pathsize-1]=='\0') pathsize--;
			strval.resize(pathsize);
		}
		else strval.clear();
		RegCloseKey(key);
		return strval;
	}

	wstring get_string(const string &value_name)
	{
		wstring wstr;
		mbtowc(value_name, wstr);
		return get_string(wstr);
	}
};

