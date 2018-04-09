#pragma once

#include "FO4\ba2arc.h"
#include "FO4\fuz.h"
#include "FO4\ilstrings.h"
#include "FO4\plugin.h"


// returns the FO4 path WITHOUT a trailing backslash
wstring GetFO4Path(HWND hwnd = nullptr)
{
	const wstring exe{L"\\fallout4.exe"};
	wstring fo4path{regkey{HKEY_LOCAL_MACHINE,
		"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 377160"}.get_string("InstallLocation")};
	if(fo4path.size() && FileExist(fo4path + exe)) return fo4path;

	fo4path = regkey{HKEY_LOCAL_MACHINE, "SOFTWARE\\Bethesda Softworks\\Fallout4"}.get_string("Installed Path");
	if(fo4path.size())
	{
		if(fo4path.back() == '\\') fo4path.pop_back();
		if(FileExist(fo4path + exe)) return fo4path;
	}

	wstring steampath{regkey{HKEY_CURRENT_USER, "Software\\Valve\\Steam"}.get_string("SteamPath")};
	if(steampath.size())
	{
		for(auto &c : steampath) if(c == '/') c = '\\';
		if(steampath.back() != '\\') steampath.push_back('\\');
		fo4path = steampath + L"steamapps\\common\\Fallout 4";
		if(FileExist(fo4path + exe)) return fo4path;

		vector<string> folders;
		ifstream f{steampath + L"config\\config.vdf"};
		while(f.good())
		{
			string line;
			getline(f, line);
			if(line.find("BaseInstallFolder") != string::npos)
			{
				auto pos2 = line.rfind('\"'), pos1 = line.rfind('\"', pos2-1)+1;
				string folder{line.substr(pos1, pos2-pos1)};
				for(auto it{folder.begin()}; it!=folder.end(); it++)
					if(*it == '\\' && *(it+1) == '\\') it = folder.erase(it);
				if(folder.back() != '\\') folder.push_back('\\');
				folders.push_back(folder);
			}
		}
		for(const auto &folder : folders)
		{
			wstring wfolder;
			mbtowc(folder, wfolder);
			fo4path = wfolder + L"steamapps\\common\\Fallout 4";
			if(FileExist(fo4path + exe)) return fo4path;
		}
	}

	static wstring initfolder;
	MessageBoxA(hwnd, "Failed to determine the Fallout 4 install location. Please locate the game folder manually. "
		R"((For example: c:\Program Files (x86)\Steam\steamapps\common\Fallout 4))", "VFRT 2", MB_ICONEXCLAMATION);
	folder_picker fp;
	if(initfolder.size()) fp.initial_folder(initfolder);
	if(fp.show(hwnd))
	{
		initfolder = fp.picked_folder();
		return fp.picked_folder();
	}
	return L"";
}