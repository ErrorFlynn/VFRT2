#pragma once

#include "..\util.h"

#include <fstream>
#include <vector>
#include <string>
#include <functional>
#include <set>

typedef unsigned short uint16;
typedef unsigned uint32;
typedef unsigned long long uint64;


class arc
{
	static const uint32 VER_FO4{0x00000001};
	static const uint32 VER_SKSE{0x00000069};
	static const uint32 SIZEMASK{0x3fffffff};
	static const uint32 COMPMASK{0xC0000000};

	uint32 ver{0};

	struct fo4_header
	{
		char	magic[4];
		uint32	ver;
		char	type[4];
		uint32	fcount;
		uint64	nametable_offset;
		bool isgeneral() { return string(type, 4) == "GNRL"; }
	} fo4_hdr;

	struct skse_header
	{
		char	magic[4];
		uint32	ver;
		uint32	offset; // offset to start of header records
		uint32	archiveFlags;
		uint32	folderCount;
		uint32	fileCount;
		uint32	totalFolderNameLength;	// Total length of all folder names, including \0s but not including the prefixed length byte.
		uint32	totalFileNameLength;	// Total length of all file names, including \0s.
		uint32	fileFlags;
	} skse_hdr{0};

#pragma pack(push, 4)
	struct fo4_entry
	{
		uint32	unk00;
		char	ext[4];
		uint32	unk08;
		uint32	unk0c;
		uint64	offset;
		uint32	packed_len;
		uint32	unpacked_len;
		uint32	unk20;
	};

	struct skse_folder_record
	{
		uint64 hash;
		uint32 count; // number of files in this folder
		uint32 unk;
		uint64 offset; // offset to name of this folder
	};

	struct skse_file_record
	{
		uint64 hash;
		uint32 size; // size of data, with a compression flag included in the value
		uint32 offset; // offset to data (from the beginning of the BSA file)

		uint32 data_size() const { return size & SIZEMASK; }
		bool compressed() const { return size & COMPMASK; }
	};
#pragma pack(pop)

	typedef std::function<bool(uint32, uint32)> cb;
	cb cbfn;
	bool kill = false, working = false;

	std::set<string> vtypes;
	std::vector<fo4_entry> fo4_entries;
	std::vector<skse_file_record> skse_entries;
	std::vector<string> names;
	string error, fname_;
	wstring fnamew_, errorw;
	const string emptystr = "";
	size_t bufidx;
	__declspec(thread) static int namecount;

public:

	arc(wstring fnamew, cb cb_fn = nullptr) : kill(false), working(false) { if(cb_fn) cbfn = cb_fn; load(fnamew); }
	arc() = default;
	~arc() { while(working); }
	bool load(wstring);
	void clear();
	string fname() { return fname_; }
	wstring fnamew() { return fnamew_; }
	void callback(cb fn) { cbfn = fn; }
	bool extract_file(string file, string &buf);
	//const string& getbuf() { return buf; }
	bool savebuf(const string &buf, wstring path);
	const auto &voice_types() { return vtypes; }
	string last_error() { return error; }
	wstring last_errorw() { mbtowc(error, errorw); return errorw; }
	size_t entry_count() { return ver == VER_FO4 ? fo4_entries.size() : skse_entries.size(); }
	bool empty() { return ver == VER_FO4 ? fo4_entries.empty() : skse_entries.empty(); }
	size_t name_count() { return names.size(); }
	size_t names_size()
	{
		size_t total(0);
		for(string &name : names) total += name.size();
		return total;
	}
	size_t bufindex() const { return bufidx; }
	const string& name_at(size_t idx) const { if(idx<names.size()) return names[idx]; else return emptystr; }
	const string& name_containing(const string&);
	const string& next_name_containing(const string&);
	const std::vector<string>& getnames() const { return names; }
};