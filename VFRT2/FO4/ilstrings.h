#include "..\util.h"

#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

typedef unsigned short uint16;
typedef unsigned uint32;
typedef unsigned long long uint64;
using namespace std;


class ilstrings
{
	struct entry
	{
		uint32	id;		// String ID
		uint32	offset;	// Offset (relative to beginning of data) to the string. Entries are not required to be sequential.
	};

	typedef function<bool(uint32, uint32)> cb;
	cb cbfn;

	uint32 entrycount;
	uint32 datasize;
	vector<entry> entries;

	wstring fnamew_, errorw;
	string error, fname_;
	unordered_map<uint32, string> table;
	int getcount;

public:
	ilstrings(wstring fname, cb cb_fn = nullptr) { if(cb_fn) cbfn = cb_fn; load(fname); }
	ilstrings(){}
	void callback(cb fn) { cbfn = fn; }
	bool load(wstring fname);
	void clear();
	string fname() { return fname_; }
	wstring fnamew() { return fnamew_; }
	const string& get(uint32 id);
	uint32 size() { return table.size(); }
	uint32 get_id_containing(string substr);
	uint32 get_next_id_containing(string substr);
	string last_error(){ return error; }
	wstring last_errorw() { mbtowc(error, errorw); return errorw; }
};