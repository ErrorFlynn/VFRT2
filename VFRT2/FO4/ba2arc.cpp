#include "ba2arc.h"
#include "zlib\zlib.h"
#include <algorithm>


bool ba2arc::load(wstring fnamew)
{
	if(!FileExist(fnamew))
	{
		string s;
		wctomb(fnamew, s);
		error = __FUNCTION__ " - file doesn't exist:\n" + s;
		return false;
	}
	ifstream f(fnamew, ios::binary);
	if(f.fail())
	{
		string s;
		wctomb(fnamew, s);
		error = __FUNCTION__ " - failed to open archive file:\n" + s + "\nError: " + GetLastErrorStr();
		return false;
	}
	clear();
	//fname_.clear();
	//fnamew_.clear();
	f.read((char*)&hdr, sizeof(hdr));
	if(!f.good())
	{
		if(f.eof()) error = __FUNCTION__ " - reached the end of file while trying to read header";
		else error = __FUNCTION__ " - failed to read header from archive\nError: " + GetLastErrorStr();
		return false;
	}
	if(!hdr.isgeneral())
	{
		error = __FUNCTION__ " - the file is a texture archive";
		return false;
	}
	
	if(hdr.fcount)
	{
		try { entries.resize(hdr.fcount); }
		catch(exception &e)
		{
			error = __FUNCTION__ " - C++ exception encountered: " + string(e.what());
			return false;
		}
		f.read((char*)&entries.front(), sizeof(entry)*hdr.fcount);
		if(!f.good())
		{
			if(f.eof()) error = __FUNCTION__ " - reached the end of file while reading file entries";
			else error = __FUNCTION__ " - failed to read file entries from archive\nError: " + GetLastErrorStr();
			return false;
		}
	}
	else
	{
		error = __FUNCTION__ " - could not find any files in archive";
		return false;
	}

	names.clear();
	if(cbfn) kill = cbfn(hdr.fcount, 0);

	int pos(1), vfiles(0);
	f.seekg(hdr.nametable_offset);

	while(f.good() && !kill)
	{
		if(cbfn) kill = cbfn(0, pos++);
		uint16 len(0);
		f.read((char*)&len, 2);
		if(len)
		{
			string name(len, '\0');
			f.read(&name.front(), len);
			name = strlower(name);
			if(name.front())
			{
				names.push_back(name);
				size_t pos(0), bslash_count(0);
				while(pos != string::npos)
				{
					pos = name.find('\\', pos+1);
					if(pos != string::npos) bslash_count++;
				}
				if(bslash_count == 4) // the voice type folder must not have a subfolder
				{
					static string last_vtype;
					string vtype = name.substr(0, name.rfind('\\'));
					vtype = vtype.substr(vtype.rfind('\\')+1);
					if(vtype != last_vtype)
					{
						vtypes.insert(vtype);
						last_vtype = vtype;
					}
				}
			}
			if(name.rfind(".fuz") != string::npos) vfiles++;
		}
	}
	
	if(!vfiles)
	{
		error = __FUNCTION__ " - could not find any voice files in archive";
		return false;
	}

	fnamew_ = fnamew;
	wctomb(fnamew_, fname_);
	error.clear();
	return true;
}


bool ba2arc::extract_file(string fname, string &buf)
{
	fname = strlower(fname);
	const entry *e(nullptr);
	for(size_t n(0), size(names.size()); n<size; n++)
	{
		string name(names[n]);
		if(name == fname)
		{
			e = &entries[n];
			bufidx = n;
			break;
		}
	}

	if(!e)
	{
		error = __FUNCTION__ " - could not find \"" + fname + "\" in archive \"" + this->fname() + "\"";
		error += "\n\nnumber of entries in archive: " + to_string(names.size());
		return false;
	}

	ifstream f(fname_, ifstream::binary);
	f.seekg(e->offset);
	buf.clear();
	auto plen(e->packed_len), uplen(e->unpacked_len);
	if(plen && plen != uplen)
	{
		buf.resize(uplen);
		string tbuf(plen, '\0');
		f.read(&tbuf.front(), plen);
		uLongf uclen(0);
		int res = uncompress((Bytef*)&buf.front(), &uclen, (Bytef*)&tbuf.front(), tbuf.size());
		if(res != Z_OK)
		{
			error = __FUNCTION__ " - failed to extract file from archive (zlib error " + to_string(res) + ")";
			return false;
		}
	}
	else
	{
		buf.resize(uplen);
		f.read(&buf.front(), uplen);
	}

	error.clear();
	return true;
}


bool ba2arc::savebuf(const string &buf, wstring path)
{
	if(path.back() != '\\') path.push_back('\\');
	string name(names[bufidx].substr(names[bufidx].rfind('\\')+1));
	wstring namew;
	mbtowc(name, namew);
	path += namew;
	ofstream f(path, ofstream::binary | ofstream::trunc);
	if(!f.is_open())
	{
		string p;
		wctomb(path, p);
		error = __FUNCTION__ " - failed to open file:\n" + p + "\n\n" + "Error: " + GetLastErrorStr();
		return false;
	}
	f.write(&buf.front(), buf.size());
	if(!f.good())
	{
		string p;
		wctomb(path, p);
		error = __FUNCTION__ " - failed to write file:\n" + p + "\n\n" + "Error: " + GetLastErrorStr();
		return false;
	}
	error.clear();
	return true;
}


__declspec(thread) int ba2arc::namecount = 0;

const string& ba2arc::name_containing(const string &substr)
{
	namecount = 0;
	working = true;
	for(const string &name : names)
		if(name.find(substr) != string::npos)
		{
			working = false;
			return name;
		}
	working = false;
	return emptystr;
}


const string& ba2arc::next_name_containing(const string &substr)
{
	__declspec(thread) static int count;
	count = 0;
	working = true;
	for(const string &name : names)
		if(name.find(substr) != string::npos)
		{
			if(count++ > namecount)
			{
				namecount++;
				working = false;
				return name;
			}
		}
	working = false;
	return emptystr;
}


void ba2arc::clear()
{
	entries.clear();
	names.clear();
	error.clear();
	errorw.clear();
	fname_.clear();
	fnamew_.clear();
}