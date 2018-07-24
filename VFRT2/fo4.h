#pragma once

#include "FO4\arc.h"
#include "FO4\fuz.h"
#include "FO4\ilstrings.h"
#include "FO4\plugin.h"

using namespace std::string_literals;

// returns the game path WITHOUT a trailing backslash
wstring GetGameFolder(bool fo4 = true)
{
	const wstring exe{fo4 ? L"\\fallout4.exe" : L"\\SkyrimSE.exe"};
	wstring game_path{regkey{HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App "s + 
		(fo4 ? "377160" : "489830")}.get_string("InstallLocation")};
	if(game_path.size() && FileExist(game_path + exe)) return game_path;

	if(fo4) game_path = regkey{HKEY_LOCAL_MACHINE, "SOFTWARE\\Bethesda Softworks\\Fallout4"}.get_string("Installed Path");
	else game_path = regkey{HKEY_LOCAL_MACHINE, "SOFTWARE\\Bethesda Softworks\\Skyrim Special Edition"}.get_string("installed path");
	if(game_path.size())
	{
		if(game_path.back() == '\\') game_path.pop_back();
		if(FileExist(game_path + exe)) return game_path;
	}

	wstring steampath{regkey{HKEY_CURRENT_USER, "Software\\Valve\\Steam"}.get_string("SteamPath")};
	if(steampath.size())
	{
		for(auto &c : steampath) if(c == '/') c = '\\';
		if(steampath.back() != '\\') steampath.push_back('\\');
		game_path = steampath + L"steamapps\\common\\" + (fo4 ? L"Fallout 4" : L"Skyrim Special Edition");
		if(FileExist(game_path + exe)) return game_path;

		std::vector<string> folders;
		std::ifstream f{steampath + L"config\\config.vdf"};
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
			game_path = wfolder + L"steamapps\\common\\" + (fo4 ? L"Fallout 4" : L"Skyrim Special Edition");
			if(FileExist(game_path + exe)) return game_path;
		}
	}
	return L"";
}

wstring BrowseForGameFolder(bool fo4 = true, HWND hwnd = nullptr)
{
	static wstring initfolder;
	MessageBoxA(hwnd, ("Failed to determine the "s + (fo4 ? "Fallout 4" : "Skyrim Special Edition") +
		" install location. Please locate the game folder manually. " + R"((For example: c:\Program Files (x86)\Steam\steamapps\common\))"
		+ (fo4 ? "Fallout 4" : "Skyrim Special Edition")).data(), "VFRT 2", MB_ICONEXCLAMATION);
	folder_picker fp;
	if(initfolder.size()) fp.initial_folder(initfolder);
	if(fp.show(hwnd))
	{
		initfolder = fp.picked_folder();
		return fp.picked_folder();
	}
	return L"";
}