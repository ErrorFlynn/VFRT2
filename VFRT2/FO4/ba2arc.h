#include "..\util.h"

#include <fstream>
#include <vector>
#include <string>
#include <functional>
#include <set>

typedef unsigned short uint16;
typedef unsigned uint32;
typedef unsigned long long uint64;
using namespace std;


class ba2arc
{
	struct header
	{
		char	magic[4];
		uint32	ver;
		char	type[4];
		uint32	fcount;
		uint64	nametable_offset;
		bool isgeneral() { return string(type, 4) == "GNRL"; }
	} hdr;

#pragma pack(push, 4)
	struct entry
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
#pragma pack(pop)

	typedef function<bool(uint32, uint32)> cb;
	cb cbfn;
	bool kill = false, working = false;

	set<string> vtypes;
	vector<entry> entries;
	vector<string> names;
	string error, fname_;
	wstring fnamew_, errorw;
	const string emptystr = "";
	size_t bufidx;
	__declspec(thread) static int namecount;

public:

	ba2arc(wstring fnamew, cb cb_fn = nullptr) : kill(false), working(false) { if(cb_fn) cbfn = cb_fn; load(fnamew); }
	ba2arc() = default;
	~ba2arc() { while(working); }
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
	size_t entry_count() { return entries.size(); }
	bool empty() { return entries.empty(); }
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
	const vector<string>& getnames() const { return names; }
};