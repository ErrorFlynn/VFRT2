#pragma once

#include "..\util.h"

#include <fstream>
#include <vector>
#include <string>
#include <functional>

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned uint32;
typedef unsigned long long uint64;
typedef int int32;


class plugin
{
	// 24 bytes
	struct GRUP
	{
		char	type[4];	// always "GRUP"
		uint32	groupSize;	// size of the entire group, including the group header (24 bytes)
		uint8	label[4];	// in case of top groups, char[4] record type 
		int32	groupType;	// 0 means top group
		uint16	stamp;
		uint16	unk1;
		uint16	version;
		uint16	unk2;
	};
	
	typedef std::function<bool(uint32 amount, uint32 value)> cb;
	cb cbfn;
	bool kill;

	filepath fname_;
	string error;
	wstring errorw;

	struct line 
	{ 
		string fname; 
		uint32 ilstring; 
		line(string s, uint32 i) : fname(s), ilstring(i) {}
		line(uint32 i, string s) : fname(s), ilstring(i) {}
	};

	std::vector<line> lines_;
	std::vector<string> vtypes;

public:
	plugin(wstring fnamew, cb cb_fn = nullptr) : kill(false) { if(cb_fn) cbfn = cb_fn; load(fnamew); }
	plugin(){}
	bool load(wstring);
	void clear();
	void callback(cb fn) { cbfn = move(fn); }
	const std::vector<line>& lines() { return lines_; }
	const filepath& path() const { return fname_; }
	string last_error(){ return error; }
	wstring last_errorw(){ mbtowc(error, errorw); return errorw; }
	const std::vector<string>& voicetypes() { return vtypes; }
};