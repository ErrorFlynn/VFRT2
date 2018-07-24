#include "plugin.h"
#include <sstream>


bool plugin::load(wstring fnamew)
{
	filepath fname(fnamew);
	if(!FileExist(fnamew))
	{
		error = __FUNCTION__ " - file doesn't exist: " + string(fname);
		return false;
	}
	auto fsize = GetFileSize(fnamew.data());
	auto vsize = string().max_size();
	if(fsize > vsize)
	{
		error = __FUNCTION__ " - file too large to load into memory: " + string(fname);
		return false;
	}

	std::ifstream f(fnamew, std::ifstream::binary);
	if(f.fail())
	{
		error = __FUNCTION__ " - failed to open plugin file:\n" + string(fname) + "\nError: " + GetLastErrorStr();
		return false;
	}
	fname_.clear();
	lines_.clear();
	clear();

	if(cbfn) kill = cbfn(fsize, 0);

	string data;
	try { data.resize(fsize, '\0'); }
	catch(std::exception &e)
	{
		error = __FUNCTION__ " - C++ exception encountered: " + string(e.what());
		return false;
	}

	try { f.read(&data.front(), fsize); }
	catch(std::exception &e)
	{
		error = __FUNCTION__ " - C++ exception encountered: " + string(e.what());
		return false;
	}

		
	auto gpos = data.find("GRUP");
	if(gpos == string::npos)
	{
		error = __FUNCTION__ " - file has no GRUP records: " + string(fname);
		return false;
	}

	uint16 ver;
	f.seekg(32);
	f.read((char*)&ver, 2);
	bool fo4{ver == 0x3f73};
	const string SUBREC{fo4 ? "TRDA" : "TRDT"};
	unsigned INFOcount(0);
	while(gpos != string::npos && !kill)
	{
		if(cbfn) kill = cbfn(0, gpos);
		GRUP g = {0};
		memcpy(&g, &data[gpos], 24);
		if(g.groupType == 7)
		{
			string igp(data.substr(gpos, g.groupSize));
			auto ipos = igp.find("INFO");
			while(ipos != string::npos)
			{
				if(cbfn) kill = cbfn(0, gpos+ipos);
				INFOcount++;
				uint32 formid(0);
				memcpy(&formid, &igp[ipos+12], 4);
				std::stringstream ss;
				ss.width(8);
				ss.fill('0');
				ss << std::hex << std::right << formid;
				string hex = ss.str();
				hex[1] = '0';

				auto tpos = igp.find(SUBREC, ipos+34);
				ipos = igp.find("INFO", ipos+1);
				while(tpos != string::npos)
				{
					if(cbfn) kill = cbfn(0, gpos+tpos);
					uint32 ilstring(*(uint32*)&igp[tpos+36-4*fo4]);
					char response_number = igp[tpos+18-8*fo4] + 0x30; // must be tpos+18 for Skyrim SE apparently
					lines_.emplace_back(ilstring, hex + '_' + response_number + ".fuz");
					tpos = igp.find(SUBREC, tpos+36-4*fo4);
					if(ipos != string::npos && tpos > ipos) tpos = string::npos;
				}

			}
		}
		gpos = data.find("GRUP", gpos+1);
	}
	
	if(INFOcount == 0)
	{
		error = __FUNCTION__ " - file doesn't appear to contain any dialogue (no INFO records were found): " + string(fname);
		return false;
	}
	error.clear();
	fname_ = fname;
	return true;
}


void plugin::clear()
{
	vtypes.clear();
	lines_.clear();
	fname_.clear();
	error.clear();
	errorw.clear();
}