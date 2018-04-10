#include "main.h"
#include "resource.h"
#include "nana_subclassing.h"
#include <Psapi.h>
#include <experimental/filesystem>
#include <nana/gui/wvl.hpp>
#include <nana/gui/widgets/progress.hpp>
#include <nana/gui/widgets/group.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/checkbox.hpp>
#include <nana/gui/filebox.hpp>
#include <nana/gui/screen.hpp>
#include <nana/gui/widgets/spinbox.hpp>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <atomic>

using namespace std::experimental::filesystem::v1;

int CALLBACK wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
#ifdef _DEBUG
	AllocConsole();
	freopen("conout$", "w", stdout);
#endif
	
	self_path = AppPath();
	inifile = self_path.dirw() + L"\\VFRT2-Settings.s";
	xenc = L'\"' + self_path.dirw() + L"\\xWMAEncode.exe\"";
	fo4dir = GetFO4Path();

	PlaySound(MAKEINTRESOURCE(IDR_BLANKWAVE), GetModuleHandle(NULL), SND_RESOURCE | SND_ASYNC);

	if(am_i_already_running())
	{
		MessageBoxA(NULL, "An instance of the program is already running! Press OK to exit.", TITLE, MB_ICONEXCLAMATION);
		return 0;
	}

	LoadSettings();
	RunGUI();
	SaveSettings();

	auto tempdir{MakeTempFolder()};
	directory_iterator dir{tempdir};
	for(auto &file : dir) DeleteFileW(file.path().wstring().data());
	RemoveDirectoryW(tempdir.data());

	return 0;
}


void RunGUI()
{
	rectangle r;
	if(conf.metric.mainfm.width == 0) r = API::make_center(1400, 800);
	else r = conf.metric.mainfm;
	form fm(r, appear::decorate<appear::minimize, appear::maximize, appear::sizable>());
	API::track_window_size(fm, {1234, 567}, false);
	if(conf.metric.mainfm.width == 0)
	{
		API::window_outline_size(fm, {fm.size().width, screen().from_window(fm).area().height});
		fm.move(fm.pos().x, 0);
	}

	fm.typeface({"Tahoma", 10});
	fm.bgcolor(colors::white);
	fm.caption(TITLE + "  :  no data"s);
	fm.icon(paint::image(wstring(self_path)));
	fm.events().unload([&fm] { conf.metric.mainfm = {fm.pos().x, fm.pos().y, fm.size().width, fm.size().height}; });
	mainform = &fm;

	coollist lb1(fm, true);
	list1 = &lb1;
	list1->events().dbl_click([](const arg_mouse &arg)
	{
		if(arg.is_left_button() && !list1->selected().empty())
		{
			auto hovered = list1->cast(arg.pos);
			if(!hovered.is_category()) PlaySelected();
		}
	});

	list1->events().mouse_up([](const arg_mouse &arg)
	{
		auto selected = list1->selected();
		auto handler = [&selected](menu::item_proxy &item)
		{
			switch(item.index())
			{
			case 0:
				ExtractSelected();
				break;

			case 1:
			case 2:
				if(selected.size() == 1) 
					CopyToClipboard(charset(list1->at(selected[0].cat)->at(selected[0].item)->text(item.index()-1), unicode::utf8), hwnd);
				else
				{
					string str;
					for(const auto &sel : selected)
					{
						if(!sel.is_category())
						{
							if(str.size()) str += "\r\n";
							str += list1->at(sel.cat)->at(sel.item)->text(item.index()-1);
						}
					}
					CopyToClipboard(charset(str, unicode::utf8), hwnd);
				}
				break;

			case 3:
				string str;
				for(const auto &sel : selected)
				{
					if(!sel.is_category())
					{
						if(str.size()) str += "\r\n";
						str += list1->at(sel.cat)->at(sel.item)->text(0);
						str += '\t' + list1->at(sel.cat)->at(sel.item)->text(1);
					}
				}
				CopyToClipboard(charset(str, unicode::utf8), hwnd);
				break;
			}
		};

		if(arg.button == mouse::right_button && !selected.empty())
		{
			paint::image img_extr, img_copyfnames, img_copydlg, img_copyrows;
			img_extr.open(ico_extr, sizeof ico_extr);
			img_copyfnames.open(ico_copyfnames, sizeof ico_copyfnames);
			img_copydlg.open(ico_copydlg, sizeof ico_copydlg);
			img_copyrows.open(ico_copyrows, sizeof ico_copyrows);
			string s;
			if(selected.size()>1) s += "s";
			coolmenu m;
			m.append("Extract voice file" + s, handler);
			m.image(0, img_extr);
			m.append("Copy file name" + s, handler);
			m.image(1, img_copyfnames);
			m.append("Copy dialogue", handler);
			m.image(2, img_copydlg);
			m.append("Copy row" + s, handler);
			m.image(3, img_copyrows);
			m.enabled(0, !extrform);
			if(API::is_window(arg.window_handle))
				m.popup_await(arg.window_handle, arg.pos.x, arg.pos.y);
		}
	});
	
	cooltree ct(fm);
	tree = &ct;

	label inf(fm);
	info = &inf;
	inf.fgcolor(color_rgb(0x696969));
	inf.typeface({"Tahoma", 10});

	button btnplay(fm);
	::btnplay = &btnplay;
	btnplay.typeface({11, "Meiryo UI"});
	btnplay.caption(u8"\u25b6");
	btnplay.bgcolor(colors::white);
	btnplay.events().click(PlaySelected);

	progbar prg(fm);
	prog = &prg;

	label lbl_filter(fm);
	lbl_filter.caption("Filter:");
	lbl_filter.fgcolor(color_rgb(0x995533));
	lbl_filter.typeface({"Verdana", 11, detail::font_style(1000)});

	textbox fltbox(fm);
	filterbox = &fltbox;
	fltbox.multi_lines(false);
	fltbox.typeface({"Courier New", 13, detail::font_style(1000)});
	fltbox.fgcolor(color_rgb(0x666666));
	fltbox.events().text_changed([](const arg_textbox &arg)
	{
		if(filterbox->caption().size() != 1)
		{
			if(conf.fnames) db.filter.filename = filterbox->caption();
			else db.filter.dialogue = filterbox->caption();
			PopulateList();
			if(db.index.empty()) filterbox->fgcolor(color_rgb(0xbb2222));
			else filterbox->fgcolor(color_rgb(0x666666));
		}
	});

	checkbox chkwords(fm), chkfnames(fm);
	chkwords.caption("Word(s)");
	chkwords.typeface({"Tahoma", 10});
	chkwords.bgcolor(colors::white);
	chkwords.check(conf.words);
	chkfnames.caption("File names");
	chkfnames.typeface({"Tahoma", 10});
	chkfnames.bgcolor(colors::white);

	chkwords.events().checked([](const arg_checkbox &arg)
	{
		conf.words = arg.widget->checked();
		if(!conf.fnames && filterbox->caption().size())
		{
			PopulateList();
		}
	});

	chkfnames.events().checked([](const arg_checkbox &arg)
	{
		if(conf.fnames = arg.widget->checked())
		{
			db.filter.dialogue.clear();
			db.filter.filename = filterbox->caption();
		}
		else
		{
			db.filter.dialogue = filterbox->caption();
			db.filter.filename.clear();
		}
		PopulateList();
	});

	button btnmenu(fm);
	btnmenu.fgcolor(color_rgb(0x666666));
	btnmenu.bgcolor(colors::white);
	btnmenu.typeface({"Meiryo UI", 18});
	btnmenu.caption(u8"\u2261");
	btnmenu.events().mouse_up([&btnmenu](const arg_mouse &arg)
	{
		auto handler = [](menu::item_proxy &item)
		{
			switch(item.index())
			{
			case 0:
				NewData();
				break;

			case 1:
				filebox fb(*mainform, true);
				fb.add_filter("VFRT2 data file (*.vfr2)", "*.vfr2");
				fb.init_path(filepath{conf.db_path}.dir());
				fb.title("Load data");
				if(fb() && FileExist(fb.file()))
				{
					conf.db_path = charset(fb.file());
					LoadData();
				}
				break;
			}
		};

		paint::image img_data, img_load;// , img_conf;
		//img_conf.open(ico_conf, sizeof ico_conf);
		img_data.open(ico_data, sizeof ico_data);
		img_load.open(ico_load, sizeof ico_load);

		coolmenu m;
		//auto conf_item = m.append("Settings", handler);
		auto data_item = m.append("New data", handler);
		auto load_item = m.append("Load data", handler);
		//m.image(conf_item.index, img_conf);
		m.image(data_item.index(), img_data);
		m.image(load_item.index(), img_load);

		size_t mh = 4 + 25*m.size(), mw = 131;
		m.popup_await(*::mainform, (btnmenu.pos().x + btnmenu.size().width) - mw, btnmenu.pos().y - mh);
	});

	fm.div(
		"<tree weight=350 margin=15>													\
		<																				\
			vert <list margin=[15,15,0]>												\
			<																			\
				bottom weight=60														\
				< vert weight=180 margin=[9] <info weight=18> <prog weight=18> >		\
				<btnplay weight=60 margin=[16,16,16,16]>								\
				<lblflt weight=85 margin=[21,10,0,30]>									\
				<fltbox margin=[16,15,16,0]> <chkwords weight=85 margin=[21,15,21]>		\
				<chkfnames weight=100 margin=[21,15,21]> <weight=80>					\
				<btnmenu weight=43 margin=[16,15,16]>									\
			>																			\
		>"
	);

	fm["tree"] << ct;
	fm["list"] << lb1;
	fm["info"] << inf;
	fm["prog"] << prg;
	fm["btnplay"] << btnplay;
	fm["lblflt"] << lbl_filter;
	fm["fltbox"] << fltbox;
	fm["chkwords"] << chkwords;
	fm["chkfnames"] << chkfnames;
	fm["btnmenu"] << btnmenu;
	fm.collocate();
	prg.hide();
	
	hwnd = (HWND)fm.native_handle();

	subclass sc(fm);
	sc.make_after(MM_MCINOTIFY, [](UINT, WPARAM, LPARAM, LRESULT*)
	{
		::btnplay->caption(u8"\u25b6");
		playing = false;
		return false;
	});

	wchar_t last_key(0);

	auto &keypress_fn = [&fm, &chkwords, &last_key](const arg_keyboard &arg)
	{
		switch(arg.key)
		{
		case keyboard::os_shift:
			if(last_key != keyboard::os_shift || list1->display_tip == false)
			{
				list1->display_tip = true;
				API::refresh_window(*list1);
			}
			break;

		case 'F':
			if(arg.ctrl) filterbox->focus();
			break;

		case 'W':
			if(arg.ctrl) chkwords.check(!chkwords.checked());
			break;

		case keyboard::escape:
			fm.close();
			break;

		case 'A':
			if(arg.ctrl && !list1->focused())
			{
				list1->focus();
				list1->events().key_press.emit(arg, *list1);
			}
			break;

		case 'N':
			if(arg.ctrl) NewData();
			break;

		case 'L':
			if(arg.ctrl)
			{
				filebox fb(*mainform, true);
				fb.add_filter("VFRT2 data file (*.vfr2)", "*.vfr2");
				fb.init_path(filepath{conf.db_path}.dir());
				fb.title("Load data");
				if(fb() && FileExist(fb.file()))
				{
					conf.db_path = charset(fb.file());
					LoadData();
				}
			}
			break;
		}
		last_key = arg.key;
	};

	auto &keyrelease_fn = [&last_key](const arg_keyboard &arg)
	{
		if(arg.key == keyboard::os_shift)
		{
			list1->display_tip = false;
			API::refresh_window(*list1);
		}
	};

	fm.events().key_press(keypress_fn);
	fm.events().key_release(keyrelease_fn);
	API::enum_widgets(fm, false, [&fm, &keypress_fn, &keyrelease_fn](widget &w)
	{
		w.events().key_press(keypress_fn);
		w.events().key_release(keyrelease_fn);
	});

	fm.show();

	thread([] { LoadData(); }).detach();
	nana::exec();
}


void NewData()
{
	if(fo4dir.empty()) fo4dir = GetFO4Path();
	plugin plug;
	ilstrings strings;
	ba2arc ba2;
	bool steps_done[3] = {false, false, false};
	const color clr1(color_rgb(0xf5f5fb)), clr2(color_rgb(0xf5fbf5)), clr3(color_rgb(0xfaf3f3)), bgbtn(color_rgb(0xeeeeee));
	const wstring datapath = fo4dir + L"\\data";
	kill = false;
	form fm(*mainform, nana::size(600, 700), appear::decorate<>());
	fm.icon(paint::image(wstring(self_path)));
	fm.bgcolor(colors::white);
	fm.caption(L"New Data");
	fm.events().unload([] {kill = true; });
	fm.div("vert <gp1 margin=[15,15,5,15] weight=150> <gp2 margin=15 weight=160> <gp3 margin=[5,15,15,15] weight=150>"
		"<gp4 margin=[5,15,15,15]> <weight=30<><btnclose weight=80><>> <weight=15>");
	button btnclose{fm, "Close"}; btnclose.bgcolor(colors::white);
	btnclose.events().click([&fm] { fm.close(); });
	fm["btnclose"] << btnclose;
	HWND hwnd = (HWND)fm.native_handle();
	group gpplugin(fm, "<bold color=0x71342F size=10 font=\"Tahoma\">Step 1: Get file names and string IDs from plugin file</>", true);
	gpplugin.bgcolor(clr1);
	gpplugin.div("vert margin=[10,10,0,15] <weight=37 margin=[5,5,0,0]<ibox1 weight=32><weight=15><info1>>"
		"<weight=62 margin=[17,5,16,0] <btnplug weight=102><weight=15><progplug>>");
	fm["gp1"] << gpplugin;
	iconbox ibox1{gpplugin, iconbox::info};
	ibox1.transparent(true);
	label info1{gpplugin, "Press the button below and select the plugin file for which you want to generate data "
		"(for example \"Fallout4.esm\"). Only plugins with localized strings are supported."};
	info1.text_align(align::left, align_v::center);
	info1.transparent(true);
	gpplugin["ibox1"] << ibox1;
	gpplugin["info1"] << info1;
	button btnplug(gpplugin, "Choose plugin");
	btnplug.bgcolor(bgbtn);

	progbar progplug(gpplugin);

	btnplug.events().click([&]
	{
		filebox fb(fm, true);
		fb.add_filter("Bethesda Plugin File (*.esm *.esp)", "*.esm;*.esp");
		fb.init_path(charset(datapath));
		if(fb())
		{
			progplug.caption("");
			btnplug.enabled(false);
			wstring plugpath = charset(fb.file());
			thread([&, plugpath]
			{
				plug.callback([&progplug](unsigned amount, unsigned value) -> bool
				{
					if(amount) progplug.amount(amount);
					else if(value) progplug.value(value);
					else progplug.inc();
					return kill;
				});

				gpplugin.bgcolor(clr1);
				ibox1.icon(iconbox::time);
				if(plug.load(plugpath))
				{
					progplug.value(progplug.amount());
					progplug.caption(plug.path().fullname() + " : " + to_string(plug.lines().size()) + " string ID + filename pairs found");
					gpplugin.bgcolor(clr2);
					ibox1.icon(iconbox::tick);
					btnplug.enabled(true);
					steps_done[0] = true;
				}
				else
				{
					steps_done[0] = false;
					progplug.value(0);
					btnplug.enabled(true);
					ibox1.icon(iconbox::info);
					MessageBoxA(hwnd, plug.last_error().data(), TITLE, MB_ICONERROR);
				}
			}).detach();
		}
	});
	
	gpplugin["btnplug"] << btnplug;
	gpplugin["progplug"] << progplug;
	
	group gpstrings(fm, "<bold color=0x71342F size=10 font=\"Tahoma\">Step 2: Get strings from localized strings file</>", true);
	gpstrings.bgcolor(clr1);
	gpstrings.div("vert margin=[10,10,0,15] <weight=37 margin=[5,5,0,0]<ibox2 weight=32><weight=15><info2>>"
		"<weight=62 margin=[17,5,16,0] <btnstrings weight=128><weight=15><progstrings>>");
	fm["gp2"] << gpstrings;
	button btnstrings(gpstrings, "Choose ilstrings file");
	btnstrings.bgcolor(bgbtn);

	progbar progstrings(gpstrings);
	iconbox ibox2{gpstrings, iconbox::info};
	ibox2.transparent(true);
	label info2{gpstrings, "Press the button below and select the localized strings file associated with "
		"the plugin file you chose in step 1 (for example \"Fallout4_en.ILSTRINGS\")."};
	info2.text_align(align::left, align_v::center);
	info2.transparent(true);

	btnstrings.events().click([&]
	{
		filebox fb(fm, true);
		fb.add_filter("Bethesda Dialogue String Table (*.ilstrings)", "*.ilstrings");
		fb.init_path(charset(datapath + L"\\strings"));
		if(fb())
		{
			progstrings.caption("");
			btnstrings.enabled(false);
			wstring stringspath = charset(fb.file());
			thread([&, stringspath]
			{
				strings.callback([&progstrings](unsigned amount, unsigned value) -> bool
				{
					if(amount) progstrings.amount(amount);
					else if(value) progstrings.value(value);
					else progstrings.inc();
					return kill;
				});

				gpstrings.bgcolor(clr1);
				ibox2.icon(iconbox::time);
				if(strings.load(stringspath))
				{
					progstrings.value(progstrings.amount());
					progstrings.caption(filepath{strings.fname()}.fullname() +" : " + to_string(strings.size()) + " strings loaded");
					gpstrings.bgcolor(clr2);
					ibox2.icon(iconbox::tick);
					btnstrings.enabled(true);
					steps_done[1] = true;
				}
				else
				{
					steps_done[1] = false;
					progstrings.value(0);
					btnstrings.enabled(true);
					ibox2.icon(iconbox::info);
					MessageBoxA(hwnd, strings.last_error().data(), TITLE, MB_ICONERROR);
				}
			}).detach();
		}
	});

	gpstrings["btnstrings"] << btnstrings;
	gpstrings["progstrings"] << progstrings;
	gpstrings["ibox2"] << ibox2;
	gpstrings["info2"] << info2;

	group gpba2(fm, "<bold color=0x71342F size=10 font=\"Tahoma\">Step 3: Get directory structure info from ba2 archive</>", true);
	gpba2.bgcolor(clr1);
	gpba2.div("vert margin=[10,10,0,15] <weight=37 margin=[5,5,0,0]<ibox3 weight=32><weight=15><info3>>"
		"<weight=62 margin=[17,5,16,0] <btnba2 weight=128><weight=15><progba2>>");
	fm["gp3"] << gpba2;
	button btnba2(gpba2, "Choose ba2 archive");
	btnba2.bgcolor(bgbtn);

	progbar progba2(gpba2);
	iconbox ibox3{gpba2, iconbox::info};
	ibox3.transparent(true);
	label info3{gpba2, "Press the button below and select the ba2 archive that contains the voice files associated "
		"with the plugin file you chose in step 1 (for example \"Fallout4 - Voices.ba2\")."};
	info3.text_align(align::left, align_v::center);
	info3.transparent(true);

	btnba2.events().click([&]
	{
		filebox fb(fm, true);
		fb.add_filter("Bethesda Archive (*.ba2)", "*.ba2");
		fb.init_path(charset(datapath));
		if(fb())
		{
			progba2.caption("");
			btnba2.enabled(false);
			wstring ba2path = charset(fb.file());
			thread([&, ba2path]
			{
				ba2.callback([&progba2](unsigned amount, unsigned value) -> bool
				{
					if(amount) progba2.amount(amount);
					else if(value) progba2.value(value);
					else progba2.inc();
					return kill;
				});

				gpba2.bgcolor(clr1);
				ibox3.icon(iconbox::time);
				if(ba2.load(ba2path))
				{
					progba2.value(progba2.amount());
					progba2.caption(filepath{ba2.fname()}.fullname() + " : " + to_string(ba2.entry_count()) + " file paths found");
					gpba2.bgcolor(clr2);
					ibox3.icon(iconbox::tick);
					API::refresh_window(gpba2);
					btnba2.enabled(true);
					steps_done[2] = true;
				}
				else
				{
					steps_done[2] = false;
					progba2.value(0);
					btnba2.enabled(true);
					ibox3.icon(iconbox::info);
					MessageBoxA(hwnd, ba2.last_error().data(), TITLE, MB_ICONERROR);
				}
			}).detach();
		}
	});
	gpba2["info3"] << info3;
	gpba2["ibox3"] << ibox3;
	gpba2["btnba2"] << btnba2;
	gpba2["progba2"] << progba2;

	group gpdata(fm, "<bold color=0x71342F size=10 font=\"Tahoma\">Step 4: Process and integrate collected data</>", true);
	gpdata.bgcolor(clr1);
	gpdata.div("vert margin=[10,10,0,15] <weight=37 margin=[5,5,0,0]<ibox4 weight=32><weight=15><info4>>"
		"<prog weight=59 margin=[15,5,16,0]>"
		"<weight=30 margin=[0,5,0,0] <btnmerge weight=270><weight=15><btnsave>>");
	fm["gp4"] << gpdata;

	progbar progdata(gpdata);
	iconbox ibox4{gpdata, iconbox::info};
	ibox4.transparent(true);
	label info4{gpdata, "The data collected in the previous steps is processed, after which you save it "
		"to a .vfr2 file. This is a CPU-intensive operation."};
	info4.text_align(align::left, align_v::center);
	info4.transparent(true);
	gpdata["prog"] << progdata;
	button btnmerge{gpdata, "Merge data into currently loaded file, and save"}, btnsave{gpdata, "Save data to new file"};
	btnmerge.bgcolor(bgbtn); btnsave.bgcolor(bgbtn);
	btnmerge.enabled(false); btnsave.enabled(false);
	gpdata["btnmerge"] << btnmerge;
	gpdata["btnsave"] << btnsave;
	gpdata["info4"] << info4;
	gpdata["ibox4"] << ibox4;

	fm.collocate();

	Data data;

	thread([&] {
		while(!kill && count(steps_done, steps_done+3, true) < 3) Sleep(100);
		if(!kill)
		{
			ibox4.icon(iconbox::time);
			btnplug.enabled(false);
			btnstrings.enabled(false);
			btnba2.enabled(false);
			data.add_game("Fallout 4");
			auto &game = data["Fallout 4"];
			auto &plugin = game += plug;
			for(const string &vtype : ba2.voice_types())
				plugin.add_voicetype(vtype);
			
			unsigned prog(0), skipped(0);
			progdata.amount(plug.lines().size());

			mutex mtx;
			size_t maxthreads(NumberOfProcessors()), start(0), end(0), finished(0), entries(0);

			auto threadproc = [&] (size_t start, size_t end)
			{
				for(size_t n(start); n<=end; n++)
				{
					auto &line = plug.lines()[n];
					if(kill) break;
					const string *path = &ba2.name_containing(line.fname);
					while(!path->empty())
					{
						if(kill) break;
						size_t pos2 = path->rfind('\\'), pos1 = path->rfind('\\', pos2-1)+1;
						string voicetype = path->substr(pos1, pos2-pos1);
						path = &ba2.next_name_containing(line.fname);
						const string &dialogue = strings.get(line.ilstring);
						if(strings.last_error().empty())
						{
							if(dialogue == " ") continue; // skip lines consisting of a single space character
							try
							{
								auto &vtype = plugin[voicetype];
								mtx.lock();
								vtype += { line.fname, dialogue };
								entries++;
								mtx.unlock();
							}
							catch(const exception&) {}
						}
					}
					progdata.value(++prog);
				}
				finished++;
			};

			if(plug.lines().size() >= maxthreads)
			{
				size_t chunk_size = plug.lines().size() / maxthreads,
					chunk_remainder = plug.lines().size() % maxthreads;
				for(size_t n(0); n<maxthreads; n++)
				{
					end += chunk_size-1;
					thread(threadproc, start, end).detach();
					start = ++end;
				}

				if(chunk_remainder) thread(threadproc, start, chunk_remainder).detach();

				while(finished < maxthreads+(chunk_remainder>0))
					Sleep(100);
			}
			else threadproc(0, plug.lines().size()-1);

			// remove any empty voice types
			for(auto &plugin : data["Fallout 4"])
				for(size_t pos(0); pos<plugin.size(); pos++)
					if(plugin[pos].empty()) plugin.erase(pos);

			if(kill) return;
			data[game][plugin].ba2name(filepath{ba2.fname()}.fullnamew());
			if(entries)
			{
				progdata.value(progdata.amount());
				gpdata.bgcolor(clr1);
				if(db.size()) btnmerge.enabled(true);
				btnsave.enabled(true);
				ibox4.icon(iconbox::save);
				progdata.caption("Finished - " + to_string(entries) + " entries created. "
					"Press a button below to save the data.");
			}
			else
			{
				gpdata.bgcolor(clr3);
				ibox4.icon(iconbox::error);
				progdata.caption("Incongruent input data produced no entries!");
			}
		}
	}).detach();

	btnmerge.events().click([&]
	{
		SetCursor(LoadCursor(0, IDC_WAIT));
		(db += data).save(conf.db_path);
		btnmerge.enabled(false);
		string key = string{data.front()} +"/" + string{data.front().at(0)};
		PopulateTree(key);
		ibox4.icon(iconbox::tick);
		string temp{progdata.caption()};
		progdata.caption("");
		gpdata.bgcolor(clr2);
		progdata.caption(temp);
		SetCursor(LoadCursor(0, IDC_ARROW));
	});

	btnsave.events().click([&]
	{
		filebox fb(fm, true);
		fb.add_filter("VFRT2 data file (*.vfr2)", "*.vfr2");
		fb.init_path(filepath{conf.db_path}.dir());
		fb.title("Save data");
		if(fb())
		{
			if(FileExist(fb.file()))
			{
				string msg{"The file \"" + filepath{fb.file()}.fullname() + "\" already exists. Do you want to overwrite it?"};
				if(MessageBoxA((HWND)fm.native_handle(), msg.data(), "Save data", MB_YESNO|MB_ICONQUESTION) == IDNO) return;
			}
			SetCursor(LoadCursor(0, IDC_WAIT));
			data.save(charset(fb.file()));
			btnsave.enabled(false);
			ibox4.icon(iconbox::tick);
			string temp{progdata.caption()};
			progdata.caption("");
			gpdata.bgcolor(clr2);
			progdata.caption(temp);
			SetCursor(LoadCursor(0, IDC_ARROW));
		}
	});

	auto &keypress_fn = [&fm](const arg_keyboard &arg) { if(arg.key == keyboard::escape) { fm.close(); } };
	fm.events().key_press(keypress_fn);
	API::enum_widgets(fm, true, [&keypress_fn](widget &w) { w.events().key_press(keypress_fn); });

	fm.modality();
}


bool am_i_already_running()
{
	HANDLE hMutex = CreateMutexA(NULL, TRUE, ("Stop right there, criminal scum!"));
	if(GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(hMutex);
		//HWND hWnd = ::FindWindow(NULL, TITLE);
		//if(hWnd) SetForegroundWindow(hWnd);
		return true;
	}
	return false;
}


void LoadSettings()
{
	if(FileExist(inifile))
	{
		ifstream data_stream(inifile, ios::binary);
		{ cereal::BinaryInputArchive iarchive(data_stream); iarchive(conf); }
	}
	else // set defaults
	{
		conf.db_path = self_path.dirw() + L"\\VFRT2-Data-EN.vfr2";
		conf.outdir = self_path.dirw() + L"\\extracted files";
		conf.maxthreads = NumberOfProcessors();
		if(conf.maxthreads == 0) conf.maxthreads = 4;
	}
}


void SaveSettings()
{
	ofstream file(inifile, ios::binary);
	{ cereal::BinaryOutputArchive oarchive(file); oarchive(conf); }
}

void Data::apply_filter()
{
	index.clear();
	string fname_filter{strlower(filter.filename)};
	string dlg_filter{strlower(filter.dialogue)};

	for(auto &plug : (*this)[filter.game])
	{
		if(filter.plugin.size() && plug.name() != filter.plugin) continue;
		for(auto &vtype : plug)
		{
			if(filter.voicetype.size() && vtype.name() != filter.voicetype) continue;
			for(auto &vfile : vtype)
			{
				if(fname_filter.size() && vfile.filename.find(fname_filter) == string::npos) continue;
				if(dlg_filter.size())
				{
					size_t pos = strlower(vfile.dialogue).find(dlg_filter);
					if(pos == string::npos) continue;
					if(conf.words)
					{
						if(pos>0 && (isalnum(vfile.dialogue[pos-1]) || vfile.dialogue[pos-1] == '\'')) continue;
						if(pos+dlg_filter.size() < vfile.dialogue.size())
							if(isalnum(vfile.dialogue[pos+dlg_filter.size()]) || vfile.dialogue[pos+dlg_filter.size()] == '\'') continue;
					}
				}
				//index.push_back(&vfile);
				size_t idx(0);
				for(; idx<index.size(); idx++) if(index[idx].name() == vfile.voicetype->name()) break;
				if(idx == index.size()) index.emplace_back(vfile.voicetype->name());
				index[idx] += &vfile;
			}
		}
	}
}


void PopulateList()
{
	auto value_translator = [](const vector<listbox::cell> &cells)
	{
		static Data::VoiceFile vfile;
		vfile.filename = cells[0].text;
		vfile.dialogue = cells[1].text;
		return &vfile;
	};

	auto cell_translator = [](const Data::VoiceFile *ptr)
	{
		vector<listbox::cell> cells;
		cells.emplace_back(ptr->filename);
		cells.emplace_back(ptr->dialogue);
		return cells;
	};
	
	list1->auto_draw(false);
	list1->erase();
	db.apply_filter();
	if(db.index.empty() && filterbox->caption().size()>1) filterbox->fgcolor(color_rgb(0xbb2222));
	else filterbox->fgcolor(color_rgb(0x666666));

	size_t total_results(0);
	for(auto &vtype : db.index)
	{
		total_results += vtype.index.size();
		auto cat = list1->append(vtype);
		cat.shared_model<recursive_mutex>(vtype.index, value_translator, cell_translator);
	}
	list1->adjust_columns();
	API::refresh_window(*list1);
	info->caption("Showing " + to_string(total_results) + " voice files");
	list1->auto_draw(true);
}


bool LoadData()
{
	if(FileExist(wstring(conf.db_path)))
	{
		info->text_align(align::center);
		info->caption("... LOADING DATA ...");
		db.load(conf.db_path);
		db.rewire();

		PopulateTree();

		info->text_align(align::left);
		mainform->caption(TITLE + "  :  "s + filepath{conf.db_path}.fullname());
		return true;
	}
	return false;
}


void PlaySelected()
{
	mciSendStringW(L"close all wait", 0, 0, 0);
	static const Data::VoiceFile *last_played(nullptr);
	auto selected = list1->selected();
	if(selected.empty())
	{
		last_played = nullptr;
		if(!playing) MessageBoxA(hwnd, "Select a voice file in the list, then click this button to play the audio."
			" Alternatively, just double-click the voice file for the same effect.", "Play voice file", MB_ICONINFORMATION);
		return;
	}

	auto vfptr = db.index[selected.front().cat-1].index[selected.front().item];
	if(vfptr == last_played && playing)
	{
		last_played = nullptr;
		return;
	}
	wstring tempdir = MakeTempFolder();
	string plugname = *vfptr->voicetype->plugin;
	string gamename = *vfptr->voicetype->plugin->game;
	wstring ba2name = db[gamename][plugname].ba2name();

	if(arcs.find(ba2name) == arcs.end())
	{
		arcs.emplace(ba2name, fo4dir + L"\\data\\" + ba2name);
		if(arcs[ba2name].last_error().size())
		{
			MessageBoxA(hwnd, arcs[ba2name].last_error().data(), "Error", MB_ICONERROR);
			arcs.erase(ba2name);
			return;
		}
	}

	static const wstring xenc = self_path.dirw() + L"\\xWMAEncode.exe";
	if(!FileExist(xenc))
	{
		MessageBoxW(hwnd, L"Could not find xWMAEncode.exe in the program directory. This file is needed "
			"for transcoding audio from xWMA to PCM.", L"Play audio", MB_ICONERROR);
		return;
	}

	ba2arc &arc = arcs[ba2name];
	string buf;
	if(!arc.extract_file("sound\\voice\\" + plugname + "\\" + vfptr->voicetype->name() + "\\" + vfptr->filename, buf))
	{
		MessageBoxA(hwnd, arc.last_error().data(), charset(ba2name).to_bytes(unicode::utf8).data(), MB_ICONERROR);
		return;
	}

	string fuzerr;
	if((fuzerr = fuz_extract(buf, tempdir, vfptr->filename)).size())
	{
		MessageBoxA(hwnd, fuzerr.data(), "fuz error", MB_ICONERROR);
		return;
	}

	wstring fnamew;
	mbtowc(vfptr->filename, fnamew);
	fnamew.replace(fnamew.rfind(L".fuz"), 4, L".wav");
	wstring mcistr = L"open waveaudio!\"" + tempdir + fnamew + L"\" alias voice1";
	last_played = vfptr;
	static timer t;
	t.interval(0);
	t.elapse([] {t.stop(); playing = true; btnplay->caption(u8"\u25a0");});
	t.start();
	mciSendStringW(mcistr.data(), 0, 0, hwnd);
	mciSendStringW(L"play voice1 notify", 0, 0, hwnd);
}


void ExtractSelected()
{
	if(extrform) { API::activate_window(*extrform); return; }

	rectangle r;
	if(conf.metric.extrfm.width == 0)
	{
		conf.metric.gpren_width = 566;
		conf.metric.lbout_width = 566;
		r = API::make_center(1200, 800);
	}
	else r = conf.metric.extrfm;

	form fm(r, appear::decorate<appear::minimize, appear::maximize, appear::sizable>());

	extrform = &fm;
	API::track_window_size(fm, {1100,600}, false);
	fm.icon(paint::image(wstring(self_path)));
	r = screen().from_window(*mainform).area();
	fm.caption("Extract voice files");
	fm.bgcolor(colors::white);

	listbox lbin(fm), lbout(fm);
	lbin.bgcolor(color_rgb(0xf8f8f8));
	lbout.bgcolor(color_rgb(0xf8f8f8));
	lbin.enable_single(true, false);
	lbin.show_header(false);
	lbin.append_header("fname");
	lbin.append_header("dlg");
	lbin.column_at(0).text_align(align::center);
	lbin.scheme().item_selected = color_rgb(0xdcefe8);
	lbin.scheme().item_highlighted = color_rgb(0xe0eaea);

	lbin.events().resized([&lbin]
	{
		lbin.column_at(0).width(140);
		lbin.column_at(1).width(lbin.size().width-25-140);
	});

	auto selected = list1->selected();
	vector<Data::VoiceType> input;
	for(auto &sel : selected) // build the input index
	{
		auto vfptr = db.index[sel.cat-1].index[sel.item];
		Data::VoiceType *vtptr(nullptr);
		for(auto &vt : input) if(vt.name() == vfptr->voicetype->name()) { vtptr = &vt; break; }
		if(!vtptr)
		{
			input.emplace_back(vfptr->voicetype->name());
			input.back().plugin = vfptr->voicetype->plugin;
			vtptr = &input.back();
		}
		vtptr->index.push_back(vfptr);
	}

	auto input_value_translator = [](const vector<listbox::cell> &cells)
	{
		static Data::VoiceFile vfile;
		vfile.filename = cells[0].text;
		vfile.dialogue = cells[1].text;
		return &vfile;
	};

	auto input_cell_translator = [](const Data::VoiceFile *ptr)
	{
		vector<listbox::cell> cells;
		cells.emplace_back(ptr->filename);
		cells.emplace_back(ptr->dialogue);
		return cells;
	};

	for(auto &vtype : input)
	{
		auto cat = lbin.append(vtype);
		cat.shared_model<recursive_mutex>(vtype.index, input_value_translator, input_cell_translator);
	}

	lbout.enable_single(true, false);
	lbout.show_header(false);
	lbout.append_header("path");
	lbout.append_header("vfptr");
	lbout.column_at(1).visible(false); // store voice file pointers here
	lbout.scheme().item_selected = lbin.scheme().item_selected;
	lbout.scheme().item_highlighted = lbin.scheme().item_highlighted;
	lbout.events().resized([&lbout](const arg_resized &arg) { lbout.column_at(0).width(lbout.size().width-25); });

	struct out_item
	{
		const Data::VoiceFile *vfptr = nullptr;
		out_item *outptr = nullptr;
		string destvt, destvf;
		out_item() = default;
		out_item(const Data::VoiceFile *ptr, string vt="", string vf="") { vfptr = ptr; destvt = move(vt); destvf = move(vf); }
	};

	vector<out_item> output;
	for(auto &vt : input) // build the output index
		for(auto vfptr : vt.index)
			output.emplace_back(vfptr, vt, vfptr->filename);
	bool live_preview(true);
	vector<out_item> output_original(output);

	auto output_value_translator = [](const vector<listbox::cell> &cells)
	{
		out_item o;
		auto &text = cells[0].text;
		size_t pos1 = text.rfind('\\'), pos2 = text.rfind('\\', pos1-1);
		o.destvf = text.substr(pos1+1);
		o.destvt = text.substr(pos2+1, pos1-pos2-1);
		const unsigned ps = sizeof o.vfptr;
		memcpy(&o.vfptr, &cells[1].text.front(), ps);
		memcpy(&o.outptr, &cells[1].text[ps], ps);
		return o;
	};

	auto output_cell_translator = [](const out_item &o)
	{
		string text = o.vfptr->voicetype->plugin->name() + '\\' + o.destvt + '\\' += o.destvf;
		vector<listbox::cell> cells;
		cells.emplace_back(move(text));
		const unsigned ps = sizeof o.vfptr;
		string str(ps*2, '\0');
		memcpy(&str.front(), &o.vfptr, ps);
		memcpy(&str[ps], &o.outptr, ps);
		cells.emplace_back(move(str));
		return cells;
	};

	lbout.at(0).shared_model<recursive_mutex>(output, output_value_translator, output_cell_translator);

	label lblin(fm), lblout(fm);
	lblin.format(true);
	lblout.format(true);
	lblin.caption("<bold color=0x71342F size=9 font=\"Tahoma\">Input:</>  <color=0x808080>( click items to rename )</>");
	string output_text = "<bold color=0x71342F size=9 font=\"Tahoma\">Output preview:</>";
	string rename_text = u8"<bold color=0x71342F size=9 font=\"Tahoma\">Renaming preview:</>";
	string noprev_text = u8"<color=0x808080>  \u066d live preview disabled (data too large) \u066d</>";
	lblout.caption(output_text);

	group gpopt(fm), gpren(fm);
	gpopt.enable_format_caption(true);
	gpren.enable_format_caption(true);
	gpopt.caption("<bold color=0x71342F size=9 font=\"Tahoma\">Extraction options</>");
	gpren.caption("<bold color=0x71342F size=9 font=\"Tahoma\">Renaming tool</>");
	gpopt.bgcolor(color_rgb(0xf8f8f8));
	gpren.bgcolor(gpopt.bgcolor());

	label lblren1(gpren, "Replace this:"), lblren2(gpren, "With this:");
	lblren1.text_align(align::right, align_v::center);
	lblren2.text_align(align::right, align_v::center);

	button btnapply(gpren, "Rename"), btncancel(gpren, "Cancel"), btnrestore(gpren, "Restore original names");
	btnapply.bgcolor(colors::white); btncancel.bgcolor(colors::white); btnrestore.bgcolor(colors::white);
	btnapply.enabled(false); btncancel.enabled(false); btnrestore.enabled(false);

	textbox tbren1(gpren), tbren2(gpren);
	tbren1.multi_lines(false); tbren2.multi_lines(false);
	tbren1.focus_behavior(widgets::skeletons::text_focus_behavior::select);
	tbren2.enabled(false);
	vector<out_item> matches;
	bool stop(false), changed(false);

	auto filter = [&]
	{
		lbout.auto_draw(false);
		string term = strlower(tbren1.caption());
		matches.clear();
		for(auto &item : output)
		{
			string vfname = item.destvf.substr(0, item.destvf.size()-4);
			if(item.destvt.find(term) != string::npos || vfname.find(term) != string::npos)
			{
				matches.push_back(item);
				matches.back().outptr = &item;
			}
		}
		live_preview = matches.size() * output.size() < 3000000u;
		if(matches.empty())
		{
			btnapply.enabled(false);
			tbren1.fgcolor(color_rgb(0xc02020));
			if(live_preview)
			{
				lblout.caption(output_text);
				lbout.at(0).shared_model<recursive_mutex>(output, output_value_translator, output_cell_translator);
			}
		}
		else
		{
			static vector<out_item> blank_vector;
			tbren1.fgcolor(colors::black);
			lblout.caption(rename_text + (live_preview ? "" : noprev_text));
			if(live_preview) lbout.at(0).shared_model<recursive_mutex>(matches, output_value_translator, output_cell_translator);
			else lbout.at(0).shared_model<recursive_mutex>(blank_vector, output_value_translator, output_cell_translator);
		}
		lbout.auto_draw(true);
		tbren2.enabled(matches.size());
		tbren2.editable(matches.size());
	};

	auto rename = [&] () -> bool
	{
		if(stop) return false;
		string term = strlower(tbren1.caption()), replacement = strlower(tbren2.caption());
		bool valid(true), collision(false), abort(false);
		const bool gui = !live_preview;
		progform pf(fm, abort);
		pf.caption("Renaming");
		pf.amount(matches.size());

		auto workfn = [&]
		{
			if(gui) while(!pf.visible()) Sleep(20);
			for(auto &match : matches)
			{
				if(gui && abort) { pf.close(); return; }
				string vtname = match.outptr->destvt,
					vfname = match.outptr->destvf.substr(0, match.outptr->destvf.size()-4);
				size_t pos1 = vtname.find(term), pos2 = vfname.find(term);
				if(pos1 != string::npos)
				{
					vtname.replace(pos1, term.size(), replacement);
					if(vtname.empty()) valid = false;
					match.destvt = move(vtname);
				}
				if(pos2 != string::npos)
				{
					vfname += match.outptr->destvf.substr(vfname.size());
					vfname.replace(pos2, term.size(), replacement);
					if(vfname.size() == 4) valid = false;
					match.destvf = move(vfname);
				}

				for(auto &out : output)
				{
					if(gui && abort) { pf.close(); return; }
					if(out.destvf == match.destvf && out.destvt == match.destvt &&
						out.vfptr->voicetype->plugin->name() == match.vfptr->voicetype->plugin->name())
					{
						if(live_preview) lblout.caption(rename_text + "   <color=0xc02020>NAME COLLISION DETECTED!</>");
						else
						{
							pf.close();
							string msg = "Name collision detected! The path \"" + match.vfptr->voicetype->plugin->name() +
								"\\" + match.destvt + "\\" + match.destvf + "\" already exists! Renaming operation aborted.";
							MessageBoxA((HWND)fm.native_handle(), msg.data(), "Error", MB_ICONERROR);
							fm.enabled(true);
							return;
						}
						valid = false;
						collision = true;
						return;
					}
				}
				pf.inc();
				if(collision) break;
				else lblout.caption(rename_text + (live_preview ? "" : noprev_text));
			}
			if(gui) pf.close();
		};

		if(gui)
		{
			thread(workfn).detach();
			pf.modality();
			return !collision;
		}

		if(live_preview)
		{
			workfn();
			btnapply.enabled(valid && matches.size());
			btncancel.enabled(matches.size());
			API::refresh_window(lbout);
		}
		return !collision;
	};

	tbren1.events().text_changed([&]
	{
		if(tbren1.caption().empty())
		{
			stop = true;
			tbren2.reset();
			stop = false;
			tbren2.enabled(false);
			matches.clear();
			lbout.auto_draw(false);
			lbout.at(0).shared_model<recursive_mutex>(output, output_value_translator, output_cell_translator);
			lbout.auto_draw(true);
			lblout.caption(output_text);
			btnapply.enabled(false);
			btncancel.enabled(false);
		}
		else
		{
			filter();
			if(!live_preview && matches.size())
			{
				btncancel.enabled(true);
				btnapply.enabled(true);
			}
			else rename();
		}
	});

	tbren2.events().focus([&tbren2, &tbren1] {if(!tbren2.enabled()) API::focus_window(tbren1); });

	auto apply = [&]
	{
		if(!live_preview) if(!rename()) return;
		for(auto &match : matches)
		{
			match.outptr->destvf = move(match.destvf);
			match.outptr->destvt = move(match.destvt);
		}
		btnrestore.enabled(true);
		lbout.auto_draw(false);
		lbout.at(0).shared_model<recursive_mutex>(output, output_value_translator, output_cell_translator);
		lbout.auto_draw(true);
		tbren1.reset();
		tbren1.focus();
	};

	btnapply.events().click([&] { apply(); });

	tbren2.events().text_changed([&] { if(live_preview) rename(); });
	tbren2.set_accept([] (wchar_t c) -> bool
	{
		string badchars(R"(<>:"/\|?*)" "\t");
		if(badchars.find(c) == string::npos) return true;
		return false;
	});

	tbren1.set_accept([&tbren1] (wchar_t c) -> bool
	{
		if(c == '\t') return false;
		return true;
	});

	tbren2.events().key_press([&](const arg_keyboard &arg)
	{
		if(arg.key == keyboard::enter && btnapply.enabled()) apply();
	});

	lbin.events().click([&lbin, &lbout, &tbren1, &tbren2, &input, &output](const arg_click &arg)
	{
		point pt = API::cursor_position();
		API::calc_window_point(lbin, pt);
		auto item = lbin.cast(pt);
		if(item.is_category())
		{
			tbren1.reset();
			tbren1.caption(lbin.at(item.cat).text());
			tbren2.focus();
		}
		else if(item.item != item.npos && lbout.selected().size())
		{
			string text = output[lbout.selected().front().item].destvf;
			tbren1.reset();
			tbren1.caption(text.substr(0, text.size()-4));
			tbren2.focus();
		}
	});

	btncancel.events().click([&tbren1] { tbren1.reset(); });

	label lblfmt(gpopt, "Output type:");
	checkbox cbfuz(gpopt, "Voice files (.fuz)"),
		cbxwm(gpopt, "xWMA audio (.xwm)"),
		cbwav(gpopt, "PCM Wave audio (.wav)");
	checkbox *cboxes[3] = {&cbfuz, &cbxwm, &cbwav};

	auto change_ext = [&output, &lbout, &tbren2, &rename]
	{
		if(conf.cvt < 3)
		{
			string newext = conf.cvt==1 ? "xwm" : (conf.cvt ? "wav" : "fuz");
			for(auto &out : output)
				for(auto itvf = out.destvf.rbegin(), itext = newext.rbegin(); *itvf!='.'; itvf++, itext++)
					*itvf = *itext;
			if(tbren2.enabled()) rename();
			else API::refresh_window(lbout);
		}
	};

	radio_group rg;
	for(auto cbox : cboxes)
	{
		rg.add(*cbox);
		cbox->events().checked([&rg, &change_ext] { conf.cvt = rg.checked(); change_ext(); });
	}
	cboxes[conf.cvt]->check(true);

	btnrestore.events().click([&]
	{
		btnrestore.enabled(false);
		for(auto &item : output)
		{
			item.destvf = item.vfptr->filename;
			item.destvt = item.vfptr->voicetype->name();
		}
		change_ext();
		tbren1.reset();
	});

	label lbldir(gpopt, "Output folder:");
	textbox tbdir(gpopt);
	tbdir.text_align(align::center);
	tbdir.caption(conf.outdir);
	tbdir.editable(false);
	tbdir.multi_lines(false);
	API::effects_edge_nimbus(tbdir, effects::edge_nimbus::none);
	button btndir(gpopt, "...");
	btndir.bgcolor(colors::white);
	lbldir.text_align(align::left, align_v::center);
	btndir.events().click([&tbdir, &fm]
	{
		folder_picker fp(conf.outdir);
		if(fp.show((HWND)fm.native_handle()))
		{
			conf.outdir = fp.picked_folder();
			tbdir.caption(conf.outdir);
		}
	});

	checkbox cblip(gpopt, "When extracting audio, also extract the lipsync data");
	cblip.check(conf.lipfiles);
	cblip.events().checked([&cblip] { conf.lipfiles = cblip.checked(); });

	button btnextract(fm, "Extract");
	btnextract.bgcolor(colors::white);
	btnextract.fgcolor(color_rgb(0x555555));
	btnextract.typeface(paint::font("Courier New", 16, detail::font_style(1000)));
	btnextract.events().mouse_enter([&btnextract] {btnextract.fgcolor(color_rgb(0x446688)); });
	btnextract.events().mouse_leave([&btnextract] {btnextract.fgcolor(color_rgb(0x555555)); });

	label lblthr(gpopt, "Max threads:");
	lblthr.text_align(align::center, align_v::center);

	spinbox sbthr{gpopt};
	sbthr.range(1, 1337, 1);
	sbthr.value(to_string(conf.maxthreads));
	sbthr.events().text_changed([&sbthr](const arg_spinbox &arg)
	{
		conf.maxthreads = sbthr.to_int();
	});

	gpopt.div("vert margin=16 <weight=27 margin=[1,0,0,0] <lblfmt weight=17%> <cbfuz weight=24%><cbxwm weight=29%><cbwav>>"
		"<weight=36 margin=[6,0] <lbldir weight=86> <tbdir margin=[0,10,0,0]> <btndir weight=24>>"
		"<weight=34 margin=[10,0,0,0] <cblip margin=[3,0,0,0] fit> <weight=130 <lblthr> <sbthr weight=50>>>");

	gpopt["lblfmt"] <<  lblfmt;
	gpopt["cbfuz"]  <<  cbfuz;
	gpopt["cbxwm"]  <<  cbxwm;
	gpopt["cbwav"]  <<  cbwav;
	gpopt["lbldir"] <<  lbldir;
	gpopt["tbdir"]  <<  tbdir;
	gpopt["btndir"]	<<  btndir;
	gpopt["cblip"]	<<  cblip;
	gpopt["sbthr"] << sbthr;
	gpopt["lblthr"] << lblthr;
	gpopt.collocate();

	gpren.div("vert margin=16"
		"<weight=34 margin=[0,0,14,0] <lblren1 weight=78 margin=[0,12,0,0]><tbren1> >"
		"<weight=34 margin=[0,0,14,0] <lblren2 weight=78 margin=[0,12,0,0]><tbren2> >"
		"<weight=30 <btnapply weight=32% margin=[0,8,0,0]><btncancel weight=24% margin=[0,8,0,8]> \
			<btnrestore margin=[0,0,0,8]> >");

	gpren["lblren1"] << lblren1;
	gpren["lblren2"] << lblren2;
	gpren["tbren1"] << tbren1;
	gpren["tbren2"] << tbren2;
	gpren["btnapply"] << btnapply;
	gpren["btncancel"] << btncancel;
	gpren["btnrestore"] << btnrestore;
	gpren.collocate();

	string divtext("vert margin=16 \
		<weight=159 <gpopt min=527 margin=[0,7,11,0]>|"+to_string(conf.metric.gpren_width)+"<gpren min=383 margin=[0,0,11,7]>>\
		<<vert min=465 <lblin weight=20 margin=[0,8,0,0]> <lbin margin=[0,8,0,0]>>|"+to_string(conf.metric.lbout_width)+"\
		<vert min=375 <lblout weight=20 margin=[0,0,0,8]> <lbout margin=[0,0,0,8]>>>\
		<btnextract weight=56 margin=[16,0,0,0]>");
	fm.div(divtext.data());

	fm["lblin"] << lblin;
	fm["lblout"] << lblout;
	fm["lbin"] << lbin;
	fm["lbout"] << lbout;
	fm["gpopt"] << gpopt;
	fm["gpren"] << gpren;
	fm["btnextract"] << btnextract;
	fm.collocate();
	
	gpopt.events().resized([&fm](const arg_resized &arg) { API::refresh_window(fm); });
	lbin.events().resized([&fm](const arg_resized &arg) { API::refresh_window(fm); });

	drawing(fm).draw([&gpopt, &lbin, &fm](paint::graphics &g)
	{
		rect rgpopt, rlbin;
		API::get_window_rectangle(gpopt, rgpopt);
		API::get_window_rectangle(lbin, rlbin);
		g.typeface(paint::font("Meiryo UI", 7));
		g.string({rgpopt.x + int(rgpopt.width) + 0, rgpopt.y + int(rgpopt.height/2) - 5}, u8"\u25c0\u25b6", colors::light_grey);
		g.string({rlbin.x + int(rlbin.width) + 1, rlbin.y + int(rlbin.height/2) - 20}, u8"\u25c0\u25b6", colors::light_grey);
	});

	fm.events().unload([&fm, &gpopt, &lbin, &lbout, &gpren]
	{
		extrform = nullptr;
		conf.metric.lbout_width = lbout.size().width-8;
		conf.metric.gpren_width = gpren.size().width-9;
		conf.metric.extrfm = {fm.pos().x, fm.pos().y, fm.size().width, fm.size().height};
	});

	auto &keypress_fn = [&fm, &btncancel, &tbren1](const arg_keyboard &arg)
	{
		if(arg.key == keyboard::escape)
		{
			if(btncancel.enabled()) { tbren1.reset(); tbren1.focus(); }
			else fm.close();
		}
	};
	fm.events().key_press(keypress_fn);
	API::enum_widgets(fm, true, [&keypress_fn](widget &w) { w.events().key_press(keypress_fn); });

	lbin.events().mouse_move([&lbin, &input, &output, &lbout, &tbren2](const arg_mouse &arg)
	{
		static const Data::VoiceFile *vfptr(nullptr), *last(nullptr);
		auto hovered = lbin.cast(point{arg.pos.x, arg.pos.y});
		if(hovered.item != hovered.npos)
		{
			vfptr = input[hovered.cat-1].index[hovered.item];
			if((output.size()<2 || vfptr != last) && !tbren2.enabled())
			{
				last = vfptr;
				size_t pos(0);
				bool found(false);
				for(auto &vt : input)
				{
					if(found) break;
					if(vfptr->voicetype->name() == vt.name())
					{
						for(auto vf_ptr : vt.index)
							if(vf_ptr == vfptr) { found = true; break; }
							else pos++;
					}
					else pos += vt.index.size();
				}
				lbout.at(0).at(pos).select(true, true);
			}
		}
	});

	lbout.events().mouse_move([&lbin, &input, &output, &lbout, &matches](const arg_mouse &arg)
	{
		if(matches.size() || output.size()<2) return;
		static const Data::VoiceFile *vfptr(nullptr), *last(nullptr);
		auto hovered = lbout.cast(point{arg.pos.x, arg.pos.y});
		if(hovered.item != hovered.npos)
		{
			vfptr = output[hovered.item].vfptr;
			if(vfptr != last)
			{
				last = vfptr;
				size_t cat{0}, pos{0};
				for(auto &vt : input)
				{
					if(vt.name() == vfptr->voicetype->name()) break;
					cat++;
				}
				for(auto vf : input[cat].index)
				{
					if(vf == vfptr) break;
					pos++;
				}
				lbin.at(cat+1).at(pos).select(true, true);
			}
		}
	});

	btnextract.events().click([&]
	{
		static const wstring xenc = self_path.dirw() + L"\\xWMAEncode.exe";
		if(!FileExist(xenc))
		{
			MessageBoxW((HWND)fm.native_handle(), L"Could not find xWMAEncode.exe in the program directory. This file is needed "
				"for transcoding audio from xWMA (.xwm) to PCM Wave (.wav).", L"Extract audio", MB_ICONERROR);
			return;
		}
		HMODULE hm = LoadLibraryW(L"msvcr100.dll");
		if(hm) FreeLibrary(hm);
		else
		{
			string msg{"A file that xWMAEncode.exe can't run without (msvcr100.dll) is missing from your computer.\n"
			"This file should be obtained by downloading and installing Microsoft Visual C++ 2010 Redistributable Package (x86).\n\n"
				"xWMAEncode.exe is necessary for transcoding audio from xWMA (.xwm) to PCM Wave (.wav)."};
			MessageBoxA((HWND)fm.native_handle(), msg.data(), "Extract audio", MB_ICONERROR);
			return;
		}

		static progform *pfptr(nullptr);
		static volatile bool abort;
		abort = false;
		static timer t;
		t.reset();
		t.interval(0);
		t.elapse([&output, &fm]
		{
			t.stop();
			progform pf(fm, abort);
			pfptr = &pf;
			pf.caption("Extracting");
			pf.amount(output.size());
			pf.modality();
			pfptr = nullptr;
		});
		t.start();

		thread([&] // this code had to be put in a detached thread, to prevent the library from blocking
		{
			for(auto &vt : input)
			{
				if(vt.plugin->game->name() == "Fallout 4" && fo4dir.empty())
				{
					Sleep(50);
					if(pfptr) pfptr->hide();
					if((fo4dir = GetFO4Path((HWND)fm.native_handle())).empty()) { if(pfptr) pfptr->close(); return; }
					else if(pfptr) pfptr->show();
				}
				wstring ba2name{vt.plugin->ba2name()}, ba2path{fo4dir + L"\\data\\" + ba2name};
				if(arcs.find(ba2name) == arcs.end())
				{
					Sleep(50);
					if(pfptr) pfptr->caption(L"Opening " + ba2name);
					arcs.emplace(ba2name, ba2path);
					if(pfptr) pfptr->caption("Extracting");
					if(arcs[ba2name].last_error().size())
					{
						Sleep(50);
						if(pfptr) pfptr->close();
						MessageBoxA((HWND)fm.native_handle(), arcs[ba2name].last_error().data(), "Error", MB_ICONERROR);
						arcs.erase(ba2name);
						return;
					}
				}
			}

			if(!FileExist(conf.outdir) && !CreateDirectoryW(conf.outdir.data(), NULL))
			{
				wstring msg{L"Failed to create output folder \"" + conf.outdir + L"\"\nError: " + GetLastErrorStrW()};
				MessageBoxW((HWND)fm.native_handle(), msg.data(), L"Error", MB_ICONERROR);
				return;
			}

			if(!tbren1.caption().empty()) tbren1.reset();
			atomic_ulong threads{0};
			size_t pos(0);
			string errors;
			static mutex mtx;

			while(!pfptr || !pfptr->visible()) Sleep(20);

			auto extrfn = [&lbout, &threads, &errors, &output, &pos](drawerbase::listbox::item_proxy item)
			{
				threads++;
				const color fgproc{colors::white}, bgproc{colors::slate_grey},
					fgdone{colors::dark_gray}, bgdone{lbout.bgcolor()};
				filepath outpath{conf.outdir + L"\\" + wstring{charset{item.text(0), unicode::utf8}}};
				auto vfptr{*reinterpret_cast<Data::VoiceFile**>(&item.text(1).front())};
				auto plugptr{vfptr->voicetype->plugin};
				auto &arc{arcs[plugptr->ba2name()]};

				mtx.lock();
				if(!FileExist(outpath.dirw()))
				{
					wstring destplug = conf.outdir + L'\\' + wstring{charset{plugptr->name(), unicode::utf8}};
					if(!FileExist(destplug))
					{
						if(!CreateDirectoryW(destplug.data(), NULL))
						{
							errors += "Failed to create output folder \"" + charset{destplug}.to_bytes(unicode::utf8)
								+ "\".\nError: " + GetLastErrorStr() + "\n\n";
							threads--;
							return;
						}
					}

					wstring destvt = destplug + L'\\' + wstring{charset{output[item.pos().item].destvt, unicode::utf8}};
					if(!FileExist(destvt))
					{
						if(!CreateDirectoryW(destvt.data(), NULL))
						{
							errors += "Failed to create output folder \"" + charset{destvt}.to_bytes(unicode::utf8)
								+ "\".\nError: " + GetLastErrorStr() + "\n\n";
							threads--;
							return;
						}
					}
				}
				mtx.unlock();

				string buf;
				string fuzpath{"sound\\voice\\" + plugptr->name() + "\\" + vfptr->voicetype->name() + "\\" + vfptr->filename};
				if(!arc.extract_file(fuzpath, buf))
				{
					errors += arc.last_error() + "\n\n";
					threads--; pos++;
					return;
				}

				if(conf.cvt == 0)
				{
					ofstream f{wstring{outpath}, ofstream::binary|ofstream::trunc};
					if(!f.good())
					{
						errors += "Failed to open file \"" + string{outpath} + "\" for writing.\nError: " + GetLastErrorStr() + "\n\n";
						threads--; pos++;
						return;
					}
					f.exceptions(ios::badbit);
					try { f.write(buf.data(), buf.size()); }
					catch(ios::failure &e)
					{
						errors += "failed to write to file \"" + string{outpath} +"\"\nError: " + e.what() + "\n\n";
						threads--; pos++;
						return;
					}
				}
				else
				{
					string error{fuz_extract(buf, outpath.dirw() + L"\\", outpath.fullname(), conf.cvt==2, conf.lipfiles)};
					if(error.size()) errors += error + "\n\n";
				}
				pos++; threads--;
			};

			chronometer t;
			for(auto item : lbout.at(0))
			{
				thread(extrfn, item).detach();
				while(threads >= conf.maxthreads) Sleep(20);
				if(pfptr && !abort && t.elapsed_ms() >= 50)
				{
					t.reset();
					pfptr->value(pos);
				}
				if(abort) break;
			}
			while(threads) Sleep(20);
			if(pfptr) pfptr->close();

			if(errors.size())
			{
				form errfm{fm, {1100, 700}, appear::decorate<appear::minimize, appear::maximize, appear::sizable>()};
				API::track_window_size(fm, {1024, 768}, false);
				errfm.icon(paint::image{wstring{self_path}});
				errfm.bgcolor(colors::white);
				errfm.caption("Errors");
				textbox tberr(errfm);
				tberr.caption(errors);
				tberr.editable(false);
				tberr.enable_caret();
				tberr.line_wrapped(true);
				API::effects_edge_nimbus(tberr, effects::edge_nimbus::none);
				errfm.div("margin=15");
				errfm[""] << tberr;
				errfm.collocate();
				errfm.show();
				auto escfn = [&errfm](const arg_keyboard &arg)
				{
					if(arg.key == keyboard::escape) errfm.close();
				};
				tberr.events().key_press(escfn);
				errfm.events().key_press(escfn);
				errfm.modality();
			}
		}).detach();
	});
	fm.modality();
}

void PopulateTree(string key_to_expand)
{
	tree->clear();
	list1->clear();
	prog->show();
	prog->unknown(true);
	chronometer t;
	long long ms(0);
	for(auto &game : db)
	{
		auto &top_node = tree->insert(game, game);
		for(auto &plugin : game)
		{
			prog->inc();
			auto &plug_node = top_node.append(plugin, plugin);
			plug_node.expand(false);
			for(auto &vtype : plugin)
			{
				plug_node.append(vtype, vtype);
				if(t.elapsed_ms() >= ms+10)
				{
					prog->inc();
					ms = t.elapsed_ms();
				}
			}
		}
	}
	prog->hide();
	db.filter.clear();
	tree->begin()->expand(true);
	db.filter.game = tree->begin()->text();
	if(key_to_expand.empty())
	{
		if(conf.selected_tree_item.size())
		{
			auto item = tree->find(conf.selected_tree_item);
			if(!item.empty())
			{
				if(item.level() == 3) item.owner().expand(true);
				else item.expand(true);
				item.select(true);
				return;
			}
		}
	}
	else
	{
		auto item = tree->find(key_to_expand);
		if(!item.empty())
		{
			item.expand(true);
			item.select(true);
			return;
		}
	}
	auto last_plugin = tree->find(string{db.front()} + "/" + string{db.front().back()});
	last_plugin.expand(true);
	last_plugin.select(true);
}
