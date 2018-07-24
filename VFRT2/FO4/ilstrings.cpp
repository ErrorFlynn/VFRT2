#include "ilstrings.h"


bool ilstrings::load(wstring fnamew)
{
	string fname;
	wctomb(fnamew, fname);
	std::ifstream f(fnamew, std::ifstream::binary);
	if(f.fail())
	{
		error = __FUNCTION__ " - failed to open strings file:\n" + fname + "\nError: " + GetLastErrorStr();
		return false;
	}
	clear();
	f.read((char*)&entrycount, 4);
	f.read((char*)&datasize, 4);
	try { entries.resize(entrycount); }
	catch(std::exception &e)
	{
		error = __FUNCTION__ " - C++ exception encountered: " + string(e.what());
		return false;
	}
	for(auto &entry : entries)
	{
		f.read((char*)&entry.id, 4);
		f.read((char*)&entry.offset, 4);
	}

	wstring esize = std::to_wstring(entries.size());
	if(cbfn) cbfn(entries.size(), 0);
	uint32 datapos = f.tellg();
	for(auto &entry : entries)
	{
		if(cbfn && cbfn(0, &entry-&entries.front())) break;
		uint32 len; // value in file includes null terminator
		string str;
		f.seekg(datapos + entry.offset);
		f.read((char*)&len, 4);
		str.resize(len-1);
		if(len>1) f.read(&str.front(), len-1);
		table.emplace(entry.id, move(str));
	}

	fnamew_ = fnamew;
	wctomb(fnamew_, fname_);
	error.clear();
	return true;
}


const string& ilstrings::get(uint32 id)
{
	string *pstr(nullptr);
	try { pstr = &table.at(id); }
	catch(const std::out_of_range&) {
		error = __FUNCTION__ " - no string exists with id " + to_hex_string(id) + " (" + std::to_string(id) + ")";
	}
	if(pstr) { error.clear(); return *pstr; }
	else return error;
}

uint32 ilstrings::get_id_containing(string substr)
{
	getcount = 0;
	for(auto &pair : table)
		if(pair.second.find(substr) != string::npos) 
			return pair.first;
	return string::npos;
}


uint32 ilstrings::get_next_id_containing(string substr)
{
	int count(0);
	for(auto &pair : table)
		if(pair.second.find(substr) != string::npos)
		{
			if(count++ > getcount)
			{
				getcount++;
				return pair.first;
			}
		}
	return string::npos;
}


void ilstrings::clear()
{
	entries.clear();
	table.clear();
	fname_.clear();
	fnamew_.clear();
	error.clear();
	errorw.clear();
}