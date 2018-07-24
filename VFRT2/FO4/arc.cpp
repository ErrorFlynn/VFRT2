#include "arc.h"
#include "zlib\zlib.h"
#include <algorithm>
#include <sstream>


bool arc::load(wstring fnamew)
{
	if(!FileExist(fnamew))
	{
		string s;
		wctomb(fnamew, s);
		error = __FUNCTION__ " - file doesn't exist:\n" + s;
		return false;
	}
	std::ifstream f(fnamew, std::ios::binary);
	if(f.fail())
	{
		string s;
		wctomb(fnamew, s);
		error = __FUNCTION__ " - failed to open archive file:\n" + s + "\nError: " + GetLastErrorStr();
		return false;
	}
	clear();
	f.seekg(4);
	f.read((char*)&ver, 4);
	f.seekg(0);

	if(ver == VER_FO4)
	{
		f.read((char*)&fo4_hdr, sizeof(fo4_hdr));
		if(!f.good())
		{
			if(f.eof()) error = __FUNCTION__ " - reached the end of file while trying to read header";
			else error = __FUNCTION__ " - failed to read header from archive\nError: " + GetLastErrorStr();
			return false;
		}


		if(!fo4_hdr.isgeneral())
		{
			error = __FUNCTION__ " - the file is a texture archive";
			return false;
		}

		if(fo4_hdr.fcount)
		{
			try { fo4_entries.resize(fo4_hdr.fcount); }
			catch(std::exception &e)
			{
				error = __FUNCTION__ " - C++ exception encountered: " + string(e.what());
				return false;
			}
			f.read((char*)&fo4_entries.front(), sizeof(fo4_entry)*fo4_hdr.fcount);
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
		if(cbfn) kill = cbfn(fo4_hdr.fcount, 0);

		int pos(1), vfiles(0);
		f.seekg(fo4_hdr.nametable_offset);

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
	}

	else if(ver == VER_SKSE)
	{
		f.read((char*)&skse_hdr, sizeof(skse_hdr));

		if(!f.good())
		{
			if(f.eof()) error = __FUNCTION__ " - reached the end of file while trying to read header";
			else error = __FUNCTION__ " - failed to read header from archive\nError: " + GetLastErrorStr();
			return false;
		}
		if((skse_hdr.fileFlags & 0x10) == 0)
		{
			error = __FUNCTION__ " - the archive does not contain voice files";
			return false;
		}

		// read file names
		string fnames(skse_hdr.totalFileNameLength, '\0');
		f.seekg(skse_hdr.offset + skse_hdr.totalFolderNameLength + skse_hdr.folderCount *
			(1 + sizeof skse_folder_record) + skse_hdr.fileCount * sizeof skse_file_record);
		f.read((char*)&fnames.front(), fnames.size());

		// read folder records
		std::vector<skse_folder_record> folders{skse_hdr.folderCount};
		f.seekg(skse_hdr.offset);
		f.read((char*)&folders.front(), skse_hdr.folderCount * sizeof skse_folder_record);

		// build the "names" and "entries" vectors
		if(cbfn) kill = cbfn(folders.size(), 0);
		names.clear();
		skse_entries.clear();
		skse_entries.reserve(skse_hdr.fileCount);
		uint32 fname_idx{0};
		for(const auto &folder : folders)
		{
			if(cbfn) kill = cbfn(0, &folder - &folders.front());
			size_t fcount{folder.count};
			uint8_t len;
			f.read((char*)&len, 1);
			string folder_name(len-1, '\0');
			f.read((char*)&folder_name.front(), len-1);
			f.ignore();
			std::vector<skse_file_record> files{fcount};
			f.read((char*)&files.front(), fcount * sizeof skse_file_record);
			skse_entries.insert(skse_entries.end(), files.begin(), files.end());
			for(const auto &file : files)
			{
				assert(fname_idx <= skse_hdr.totalFileNameLength);
				string fname(fnames.data() + fname_idx);
				fname_idx += fname.size() + 1;
				names.emplace_back(folder_name + '\\' + fname);
				size_t pos(0), bslash_count(0);
				const string &name = names.back();
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
		}
	}
	else
	{
		string s;
		wctomb(fnamew, s);
		error = __FUNCTION__ " - unsupported archive version: " + s;
		return false;
	}

	fnamew_ = fnamew;
	wctomb(fnamew_, fname_);
	error.clear();
	return true;
}


bool arc::extract_file(string fname, string &buf)
{
	fname = strlower(fname);

	if(ver == VER_FO4)
	{
		const fo4_entry *e(nullptr);
		for(size_t n(0), size(names.size()); n<size; n++)
		{
			string name(names[n]);
			if(name == fname)
			{
				e = &fo4_entries[n];
				bufidx = n;
				break;
			}
		}

		if(!e)
		{
			error = __FUNCTION__ " - could not find \"" + fname + "\" in archive \"" + this->fname() + "\"";
			error += "\n\nnumber of entries in archive: " + std::to_string(names.size());
			return false;
		}

		std::ifstream f(fname_, std::ifstream::binary);
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
				error = __FUNCTION__ " - failed to extract file from archive (zlib error " + std::to_string(res) + ")";
				return false;
			}
		}
		else
		{
			buf.resize(uplen);
			f.read(&buf.front(), uplen);
		}
	}
	else if(ver == VER_SKSE)
	{
		const skse_file_record *e(nullptr);
		for(size_t n(0), size(names.size()); n<size; n++)
		{
			string name(names[n]);
			if(name == fname)
			{
				e = &skse_entries[n];
				bufidx = n;
				break;
			}
		}

		if(!e)
		{
			error = __FUNCTION__ " - could not find \"" + fname + "\" in archive \"" + this->fname() + "\"";
			error += "\n\nnumber of entries in archive: " + std::to_string(names.size());
			return false;
		}

		std::ifstream f(fname_, std::ifstream::binary);
		f.seekg(e->offset);
		buf.clear();
		const auto data_size{e->data_size()};

		if(e->compressed())
		{
			buf.resize(data_size*10);
			string tbuf(data_size, '\0');
			f.read(&tbuf.front(), data_size);
			uLongf uclen(0);
			int res = uncompress((Bytef*)&buf.front(), &uclen, (Bytef*)&tbuf.front(), tbuf.size());
			if(res != Z_OK)
			{
				error = __FUNCTION__ " - failed to extract file from archive (zlib error " + std::to_string(res) + ")";
				return false;
			}
			buf.resize(uclen);
		}
		else
		{
			buf.resize(data_size);
			f.read(&buf.front(), data_size);
		}
	}
	else
	{
		std::stringstream ss;
		ss << std::hex << ver;
		error = __FUNCTION__ " - unsupported archive version: 0x" + ss.str();
		return false;
	}

	error.clear();
	return true;
}


bool arc::savebuf(const string &buf, wstring path)
{
	if(path.back() != '\\') path.push_back('\\');
	string name(names[bufidx].substr(names[bufidx].rfind('\\')+1));
	wstring namew;
	mbtowc(name, namew);
	path += namew;
	std::ofstream f(path, std::ofstream::binary | std::ofstream::trunc);
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


__declspec(thread) int arc::namecount = 0;

const string& arc::name_containing(const string &substr)
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


const string& arc::next_name_containing(const string &substr)
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


void arc::clear()
{
	skse_entries.clear();
	fo4_entries.clear();
	names.clear();
	error.clear();
	errorw.clear();
	fname_.clear();
	fnamew_.clear();
}