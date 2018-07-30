#include <nana/gui.hpp>
#include <nana/gui/widgets/listbox.hpp>
#include <nana/gui/timer.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/progress.hpp>
#include <nana/gui/widgets/treebox.hpp>
#include <nana/gui/widgets/textbox.hpp>
#include <nana/gui/widgets/menu.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

#include "util.h"
#include "fo4.h"
#include "icons.h"
#include "FO4\zlib\zlib.h"

#include <string>
#include <map>

#pragma warning( disable : 4244 4800 4267 4996)
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
	processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define TITLE "Voice File Reference Tool v2.1.2"
#define TITLEW L"Voice File Reference Tool v2.1.2"


wstring inifile, fo4dir, sksedir, xenc;
filepath self_path;
std::map<wstring, arc> arcs;
HWND hwnd;
nana::label *info(nullptr);
nana::textbox *filterbox(nullptr);
nana::button *btnplay;
nana::form *mainform, *extrform(nullptr);
bool kill(false), playing(false);

void PopulateList();
void PopulateTree(string key_to_expand = "");
void NewData(bool fo4 = true);
bool LoadData();
void LoadSettings();
void SaveSettings();
void RunGUI();
void PlaySelected();
void ExtractSelected();
bool am_i_already_running();

struct rect : public nana::rectangle
{
	rect() : nana::rectangle() {}
	rect(int x, int y, unsigned width, unsigned height) : nana::rectangle(x, y, width, height) {}

private:
	friend cereal::access;
	template<class Archive>
	void serialize(Archive &archive) { archive(x, y, width, height); }
};

struct
{
	bool words = false, fnames = false, lipfiles = false;
	unsigned cvt = 0, maxthreads = 0;
	wstring db_path, outdir;
	string selected_tree_item;

	struct
	{
		rect mainfm, extrfm, data;
		unsigned gpren_width, lbout_width;

	private:
		friend cereal::access;
		template<class Archive>
		void serialize(Archive &archive) { archive(mainfm, extrfm, data, gpren_width, lbout_width); }
	} metric;

private:
	friend cereal::access;
	template<class Archive>
	void serialize(Archive &archive) { archive(words, fnames, lipfiles, cvt, maxthreads, db_path, outdir, selected_tree_item, metric); }
} conf;


class Data
{
	string error;
	
public:

	class Game;
	class VoiceType;
	class Plugin;

	struct VoiceFile
	{
		string filename, dialogue;
		VoiceType *voicetype = nullptr;
		VoiceFile() {}
		VoiceFile(string fname, string dlg) { filename = move(fname); dialogue = move(dlg); }
		
	private:

		friend cereal::access;
		template<class Archive>
		void serialize(Archive &archive) { archive(filename, dialogue); }
	};

	class VoiceType
	{
		string name_;
		std::vector<VoiceFile> voicefiles_;

		friend cereal::access;
		template<class Archive>
		void serialize(Archive &archive) { archive(name_, voicefiles_); }

	public:

		std::vector<const VoiceFile*> index;
		Plugin *plugin = nullptr;

		VoiceType() {}
		VoiceType(string name) { name_ = name; }
		const string &name() const noexcept { return name_; }
		operator string() const noexcept { return name_; }
		bool operator==(string name) const noexcept { return name_ == name; }
		void operator+=(VoiceFile vfile) { voicefiles_.push_back(std::move(vfile)); }
		void operator+=(VoiceFile *ptr) { index.push_back(ptr); }
		VoiceFile &operator[] (const size_t pos) { return voicefiles_[pos]; }

		auto begin() noexcept { return voicefiles_.begin(); }
		auto end() noexcept { return voicefiles_.end(); }
		auto begin() const noexcept { return voicefiles_.begin(); }
		auto end() const noexcept { return voicefiles_.end(); }
		auto size() const noexcept { return voicefiles_.size(); }
		bool empty() const noexcept { return voicefiles_.empty(); }
	};

	class Plugin
	{
		string name_, error;
		wstring path_, arcname_;
		std::vector<VoiceType> voicetypes_;

		friend Data;
		friend cereal::access;
		template<class Archive>
		void serialize(Archive &archive) { archive(name_, path_, voicetypes_, arcname_); }

	public:

		Game *game = nullptr;

		Plugin() {}
		Plugin(filepath path) { path_ = strlower(wstring(path)); name_ = filepath(path_).fullname(); }
		const string &name() const noexcept { return name_; }
		const wstring &arcname() const noexcept { return arcname_; }
		void arcname(wstring s) { arcname_ = s; }
		operator string() const noexcept { return name_; }
		bool operator==(string name) const { return name_ == strlower(name); }
		VoiceType& operator[] (size_t pos) { return voicetypes_[pos]; }

		VoiceType& operator[] (const string &name)
		{
			for(auto &voicetype : voicetypes_)
				if(voicetype == name) return voicetype;
			error = __FUNCTION__ " - voice type \"" + name + "\" not found.";
			throw std::out_of_range(error);
		}

		auto begin() noexcept { return voicetypes_.begin(); }
		auto end() noexcept { return voicetypes_.end(); }
		auto begin() const noexcept { return voicetypes_.begin(); }
		auto end() const noexcept { return voicetypes_.end(); }
		VoiceType& front() { return voicetypes_.front(); }
		VoiceType& back() { return voicetypes_.back(); }
		auto size() const noexcept { return voicetypes_.size(); }
		auto at(size_t pos) { return voicetypes_.at(pos); }
		auto erase(size_t pos) { return voicetypes_.erase(voicetypes_.begin()+pos); }
		auto path() const noexcept { return path_; }

		bool add_voicetype(const string &name)
		{
			for(auto &voicetype : voicetypes_) if(voicetype == name) return false;
			voicetypes_.push_back(name);
			return true;
		}
	};

	class Game
	{
		string name_;
		std::vector<Plugin> plugins_;
		string error;

		friend cereal::access;
		template<class Archive>
		void serialize(Archive &archive)
		{
			archive(name_, plugins_);
		}

	public:
		
		Game() {  }
		Game(string name) { name_ = name; }
		const string &name() const { return name_; }
		operator string() { return name_; }
		bool operator==(string name) { return strlower(name_) == strlower(name); }
		Plugin& operator+=(string name) { add_plugin(name); return operator[](name); }
		Plugin& operator+=(const plugin &p) { add_plugin(p); return operator[](p); }
		Plugin& font() { return plugins_.front(); }
		Plugin& back() { return plugins_.back(); }
		auto size() const noexcept { return plugins_.size(); }
		auto begin() noexcept { return plugins_.begin(); }
		auto end() noexcept { return plugins_.end(); }
		auto begin() const noexcept { return plugins_.begin(); }
		auto end() const noexcept { return plugins_.end(); }
		auto at(size_t pos) { return plugins_.at(pos); }
		auto erase(size_t pos) { return plugins_.erase(plugins_.begin()+pos); }
		auto insert(size_t pos, Data::Plugin &plug) { return plugins_.insert(plugins_.begin()+pos, plug); }

		bool add_plugin(filepath path)
		{
			for(auto &p : plugins_) if(p == path.fullname()) return false;
			plugins_.push_back(path);
			return true;
		}

		bool add_plugin(const plugin &p) { return add_plugin(p.path()); }

		Plugin& operator[] (const string &name)
		{
			for(auto &plugin : plugins_)
				if(plugin == name) return plugin;
			error = __FUNCTION__ " - plugin \"" + name + "\" not found.";
			throw std::out_of_range(error);
		}

		Plugin& operator[] (const plugin &p) { return operator[](strlower(p.path().fullname())); }
	};

private:

	std::vector<Game> games;

	friend cereal::access;
	template<class Archive>
	void serialize(Archive &archive) { archive(games); }

public:

	std::vector<VoiceType> index;

	struct filter_
	{
		string game, plugin, voicetype, filename, dialogue;
		void clear()
		{
			game.clear(); plugin.clear(); filename.clear();
			voicetype.clear(); dialogue.clear();
		}
	} filter;

	bool add_game(const string &name)
	{
		for(auto &game : games) if(game == name) return false;
		games.push_back(name);
		return true;
	}

	// throws if game doesn't exist
	Game &operator[] (const string &name)
	{
		for(auto &game : games)
			if(game == name) return game;
		error = __FUNCTION__ " - game \"" + name + "\" not found.";
		throw std::out_of_range(error);
	}

	// adds game if it doesn't exist
	Game &at(const string &name)
	{
		Game *g{nullptr};
		try { g = &(*this)[name]; }
		catch(std::out_of_range e) { add_game(name); g = &this->back(); }
		return *g;
	}

	void rewire()
	{
		for(auto &game : games)
			for(auto &plugin : game)
			{
				plugin.game = &game;
				for(auto &voicetype : plugin)
				{
					voicetype.plugin = &plugin;
					for(auto &voicefile : voicetype)
						voicefile.voicetype = &voicetype;
				}
			}
	}

	void apply_filter();

	auto erase(size_t pos) { return games.erase(games.begin()+pos); }
	auto insert(size_t pos, Data::Game &game) { return games.insert(games.begin()+pos, game); }
	auto begin() noexcept { return games.begin(); }
	auto begin() const noexcept { return games.begin(); }
	auto end() noexcept { return games.end(); }
	auto end() const noexcept { return games.end(); }
	Game& front() { return games.front(); }
	Game& back() { return games.back(); }
	auto size() const noexcept { return games.size(); }
	void clear() { index.clear(); games.clear(); }

	void save(wstring path, bool compress = true)
	{
		if(compress)
		{
			std::stringstream ss;
			{ cereal::BinaryOutputArchive oarchive(ss); oarchive(*this); }
			string dest(ss.str().size(), '\0');
			uLongf destsize(ss.str().size());
			LONGLONG uncompressed_size(destsize);
			int res = compress2((Bytef*)&dest.front(), &destsize, (Bytef*)&ss.str().front(), ss.str().size(), Z_BEST_COMPRESSION);
			dest.resize(destsize);
			std::ofstream file(path, std::ios::binary);
			file.write((const char*)&uncompressed_size, sizeof uncompressed_size);
			file.write(dest.data(), dest.size());
		}
		else
		{
			std::ofstream file(path, std::ios::binary);
			{ cereal::BinaryOutputArchive oarchive(file); oarchive(*this); }
		}
	}

	void load(wstring path, bool decompress = true)
	{
		if(decompress)
		{
			std::ifstream file(path, std::ios::binary);
			LONGLONG uncompressed_size(0);
			file.read((char*)&uncompressed_size, sizeof uncompressed_size);
			LONGLONG fsize = GetFileSize(path.data()) - sizeof uncompressed_size;
			string src(fsize, '\0'), dest(uncompressed_size, '\0');
			file.read(&src.front(), fsize);
			uLongf destsize(dest.size());
			int res = uncompress((Bytef*)&dest.front(), &destsize, (Bytef*)&src.front(), src.size());
			if(res == 0)
			{
				clear();
				dest.resize(destsize);
				std::stringstream ss(dest);
				{ cereal::BinaryInputArchive iarchive(ss); iarchive(*this); }
			}
		}
		else
		{
			clear();
			std::ifstream data_stream(path, std::ios::binary);
			{ cereal::BinaryInputArchive iarchive(data_stream); iarchive(*this); }
		}
	}

	Data &operator +=(const Data &other)
	{
		for(auto &other_game : other)
		{
			auto &game{at(other_game.name())};
			for(auto &other_plugin : other_game)
			{
				try { game[other_plugin.name()]; }
				catch(std::out_of_range&) { game.add_plugin(filepath{other_plugin.path()}); }
				auto &plugin = game[other_plugin.name()];
				plugin.arcname(other_plugin.arcname());
				plugin.voicetypes_ = other_plugin.voicetypes_;
				/*for(auto &other_vt : other_plugin)
				{

				}*/
			}
		}
		rewire();
		return *this;
	}
} db;


class coolmenu : public nana::menu
{
	class menu_renderer : public nana::menu::renderer_interface
	{

	public:
		menu_renderer(const nana::pat::cloneable<renderer_interface>& rd) : reuse_(rd) {}

	private:
		void background(graph_reference graph, nana::window wd) override
		{
			graph.rectangle(true, nana::colors::white); // entire area
			graph.rectangle({1,1,28,graph.height()-2}, true, nana::color_rgb(0xf6f6f6)); // icon area
			graph.rectangle(false, static_cast<nana::color_rgb>(0xa0b0c0)); // border
		}

		void item(graph_reference graph, const nana::rectangle& r, const attr & atr) override
		{
			if(state::active == atr.item_state)
			{
				graph.rectangle(r, true, static_cast<nana::color_rgb>(0xDCEFE8));
				graph.rectangle(r, false, static_cast<nana::color_rgb>(0x9CD1BC));
			}
		}

		void item_image(graph_reference graph, const nana::point& pos, unsigned image_px, const nana::paint::image& img) override
		{
			reuse_->item_image(graph, pos, image_px, img);
		}

		void item_text(graph_reference graph, const nana::point& pos, const string& text, unsigned pixels, const attr& atr) override
		{
			reuse_->item_text(graph, pos, text, pixels, atr);
		}

		void sub_arrow(graph_reference graph, const nana::point& pos, unsigned pixels, const attr & atr) override
		{
			reuse_->sub_arrow(graph, pos, pixels, atr);
		}
	private:
		nana::pat::cloneable<renderer_interface> reuse_;
	};

public:
	coolmenu() { renderer(menu_renderer{renderer()}); }
};


class progbar : public nana::progress
{
public:

	progbar() : progress() {}
	progbar(nana::window w, bool visible = true) { create(w, visible); }

	bool create(nana::window w, bool visible)
	{
		if(progress::create(w, visible))
		{
			scheme().gradient_bgcolor = nana::color_rgb(0xf5f5f5);
			scheme().gradient_fgcolor = nana::color_rgb(0xb7d3db);
			scheme().background = nana::color_rgb(0xe4e4e4);
			scheme().foreground = nana::color_rgb(0x64a3b5);

			nana::drawing{*this}.draw([this](nana::paint::graphics &graph)
			{
				if(caption().size())
				{
					static const nana::paint::font font{"Arial Bold", 10, nana::detail::font_style()};
					HDC dc = (HDC)graph.context();
					if(dc)
					{
						graph.typeface(font);
						RECT rect = {0, 0, graph.width(), graph.height()};
						SetTextColor(dc, RGB(0xff, 0xff, 0xff));
						DrawTextA(dc, caption().data(), -1, &rect, DT_CENTER|DT_SINGLELINE|DT_VCENTER);
					}
				}
			});
			return true;
		}
		return false;
	}
} *prog;


class cooltree : public nana::treebox
{
	std::vector<item_proxy> top_nodes_;
	string last_selected;

	class tree_placer : public compset_placer_interface
	{
		using cloneable_placer = nana::pat::cloneable<treebox::compset_placer_interface>;
		cloneable_placer placer_;

	public:
		tree_placer(const cloneable_placer& r) : placer_(r) {}

	private:

		virtual void enable(component_t comp, bool enabled) override
		{
			placer_->enable(comp, enabled);
		}

		virtual bool enabled(component_t comp) const override
		{
			return placer_->enabled(comp);
		}

		virtual unsigned item_height(graph_reference graph) const override
		{
			return placer_->item_height(graph);
		}

		virtual unsigned item_width(graph_reference graph, const item_attribute_t& attr) const override
		{
			unsigned width = graph.size().width-2 - tree->scrollbar()*16 - !attr.has_children*8;
			if(attr.has_children)
			{
				if(!tree->is_top_node(attr.text)) width -= 8;
			}
			else width -= 8;
			return width;
		}

		virtual bool locate(component_t comp, const item_attribute_t& attr, nana::rectangle * r) const override
		{
			return placer_->locate(comp, attr, r);
		}
	};


	class tree_renderer : public nana::drawerbase::treebox::renderer_interface
	{
		using cloneable_renderer = nana::pat::cloneable<treebox::renderer_interface>;
		cloneable_renderer renderer_;

	public:
		tree_renderer(const cloneable_renderer & rd) : renderer_(rd) {}

	private:
		
		void begin_paint(widget& wdg) override {}

		void bground(graph_reference graph, const compset_interface * compset) const override
		{
			comp_attribute_t attr;

			if(compset->comp_attribute(component::bground, attr))
			{
				const nana::color color_table[][2] = {{{0xf9, 0xf9, 0xf9},{0xe0, 0xe0, 0xe0}}, //highlighted
					{{nana::colors::white},{0xa8, 0xa8, 0xa8}}, //Selected and highlighted
					{{nana::colors::white},{0xb0, 0xb0, 0xb0}} //Selected but not highlighted
				};

				const nana::color *clrptr = nullptr;
				if(compset->item_attribute().mouse_pointed)
				{
					if(compset->item_attribute().selected)
					{
						if(tree->is_top_node(compset->item_attribute().text))
						{
							graph.rectangle({4, attr.area.y, graph.size().width-7-tree->scrollbar()*16,
								attr.area.height}, true, nana::color_rgb(0xf9f9f9));
							return;
						}
						clrptr = color_table[1];
					}
					else if(tree->is_top_node(compset->item_attribute().text))
					{
						graph.rectangle({4, attr.area.y, graph.size().width-7-tree->scrollbar()*16,
							attr.area.height}, true, nana::color_rgb(0xf9f9f9));
						return;
					}
					else clrptr = color_table[0];
				}
				else if(compset->item_attribute().selected)
				{
					if(tree->is_top_node(compset->item_attribute().text))
					{
						graph.rectangle({4, attr.area.y, graph.size().width-7-tree->scrollbar()*16,
							attr.area.height}, true, nana::colors::white);
						return;
					}
					clrptr = color_table[2];
				}
				else if(tree->is_top_node(compset->item_attribute().text))
				{
					graph.rectangle({4, attr.area.y, graph.size().width-7-tree->scrollbar()*16,
						attr.area.height}, true, nana::color_rgb(0xebedf0));
					return;
				}

				if(clrptr)
				{
					attr.area.width = graph.size().width-2 - tree->scrollbar()*16;
					attr.area.x = 1;
					graph.round_rectangle(attr.area, 1, 1, clrptr[1], false, nana::colors::black);
					graph.rectangle(attr.area.pare_off(2), true, *clrptr);
				}
			}
		}

		void expander(graph_reference graph, const compset_interface * compset) const override
		{
			comp_attribute_t attr;
			auto item = compset->item_attribute();
			if(compset->comp_attribute(component::expander, attr))
			{
				nana::facade<nana::element::arrow> arrow("solid_triangle");
				arrow.direction(nana::direction::southeast);
				if(!item.expended)
				{
					arrow.switch_to("hollow_triangle");
					arrow.direction(nana::direction::east);
				}
				auto r = attr.area;
				if(item.has_children)
				{
					if(tree->is_top_node(item.text)) r.x = 0;
					else r.x = 10;
				}
				else r.x = 20;
				r.y += (attr.area.height - 16) / 2;
				r.width = r.height = 16;
				arrow.draw(graph, tree->bgcolor(), (attr.mouse_pointed ? nana::colors::light_sea_green : nana::colors::black), 
					r, nana::element_state::normal);
			}
		}

		void crook(graph_reference graph, const compset_interface * compset) const override
		{
			renderer_->crook(graph, compset);
		}

		virtual void icon(graph_reference graph, const compset_interface * compset) const override
		{
			renderer_->icon(graph, compset);
		}

		virtual void text(graph_reference graph, const compset_interface * compset) const override
		{
			comp_attribute_t attr;
			static const nana::paint::font normal_font{"Tahoma", 10};
			const string text = compset->item_attribute().text;
			const bool selected = compset->item_attribute().selected;

			if(compset->comp_attribute(component::text, attr))
			{
				nana::color textcolor = selected ? nana::color_rgb(0x843928) : tree->fgcolor();
				if(compset->item_attribute().has_children)
				{
					if(tree->is_top_node(text))
					{
						auto &offset_color = [](const nana::color &c, const int offset)
						{
							unsigned
								r = c.r() + offset,
								g = c.g() + offset,
								b = c.b() + offset;
							return nana::color(r, g, b);
						};
						const bool first_node{text == tree->top_nodes_.front().text()};
						textcolor = nana::color_rgb(0x667788);
						graph.typeface({"Franklin Gothic Medium", 15});
						nana::color outline_color = nana::color_rgb(0xf4fafe);
						int outline_size = 2;
						
						graph.string({20, attr.area.y -1 -outline_size+first_node}, text, outline_color);
						graph.string({20, attr.area.y -1 +outline_size+first_node}, text, outline_color);
						graph.string({20-outline_size, attr.area.y -1 -outline_size+first_node}, text, outline_color);
						graph.string({20+outline_size, attr.area.y -1 -outline_size+first_node}, text, outline_color);
						graph.string({20-outline_size, attr.area.y -1 +outline_size+first_node}, text, outline_color);
						graph.string({20+outline_size, attr.area.y -1 +outline_size+first_node}, text, outline_color);
						if(outline_size > 1)
						{
							outline_color = offset_color(outline_color, -25);
							outline_size = 1;
							graph.string({20, attr.area.y -1 -outline_size+first_node}, text, outline_color);
							graph.string({20, attr.area.y -1 +outline_size+first_node}, text, outline_color);
							graph.string({20-outline_size, attr.area.y -1 -outline_size+first_node}, text, outline_color);
							graph.string({20+outline_size, attr.area.y -1 -outline_size+first_node}, text, outline_color);
							graph.string({20-outline_size, attr.area.y -1 +outline_size+first_node}, text, outline_color);
							graph.string({20+outline_size, attr.area.y -1 +outline_size+first_node}, text, outline_color);
						}
						graph.string({20, attr.area.y -1 + first_node}, text, textcolor); // original
						graph.typeface(normal_font);
							
						bool draw_top_line{true};
						size_t idx{0};
						while(tree->top_nodes_[idx].text() != compset->item_attribute().text) idx++;
						if(idx > 0) 
							if(tree->top_nodes_[--idx]._m_node()->is_ancestor_of(tree->selected()._m_node()))
								if(!tree->selected().expanded())
									draw_top_line = !tree->selected().sibling().empty();
						if(tree->top_nodes_[idx].selected() && !tree->top_nodes_[idx].expanded())
							draw_top_line = false;

						const nana::color line_color{selected ? nana::color_rgb(0xd0d7d9) : nana::color_rgb(0xd6dddf)};

						if(draw_top_line)
						{
							graph.line({4, attr.area.y-(1-first_node)}, {(int)graph.size().width-4-tree->scrollbar()*16,
								attr.area.y-(1-first_node)}, line_color);
							graph.line({4, attr.area.y-(2-first_node)}, {(int)graph.size().width-4-tree->scrollbar()*16,
								attr.area.y-(2-first_node)}, line_color);
						}
						if(!tree->find(text).child().selected())
						{
							graph.line({4, attr.area.y+23}, {(int)graph.size().width-4-tree->scrollbar()*16,
								attr.area.y+23}, line_color);
							graph.line({4, attr.area.y+24}, {(int)graph.size().width-4-tree->scrollbar()*16,
								attr.area.y+24}, line_color);
						}
						return;
					}
					else
					{
						if(selected) textcolor = nana::color_rgb(0x995533);
						else textcolor = nana::color_rgb(0x667788);
						static const nana::paint::font boldfont{"Tahoma", 10, nana::detail::font_style(1000)};
						graph.typeface(boldfont);

						// workaround for what looks like a bug in the library
						// (`string` method sometimes ignores the color argument)
						HDC dc{(HDC)graph.context()};
						SetTextColor(dc, RGB(textcolor.r(), textcolor.g(), textcolor.b()));

						graph.string({30, attr.area.y + 3}, text, textcolor);
						graph.typeface(normal_font);
					}
				}
				else
				{
					graph.typeface(normal_font);
					//graph.string({40, attr.area.y + 3}, text, textcolor);
					HDC dc{(HDC)graph.context()};
					RECT rect = {40, attr.area.y, 40+attr.area.width, attr.area.y + attr.area.height};
					SetTextColor(dc, RGB(textcolor.r(), textcolor.g(), textcolor.b()));
					DrawTextA(dc, text.data(), -1, &rect, DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
				}
			}
		}
	};


public:

	auto &top_nodes() { return top_nodes_; }

	cooltree(nana::window w, bool visible = true) : treebox(w, visible)
	{
		typeface(nana::paint::font("Tahoma", 10));
		bgcolor(nana::color_rgb(0xf0f0f0));
		renderer(tree_renderer{renderer()});
		placer(tree_placer{placer()});

		events().selected([this](const nana::arg_treebox &arg)
		{
			conf.selected_tree_item = tree->make_key_path(arg.item, "/");
			if(arg.item.text() == last_selected) return;
			last_selected = arg.item.text();

			switch(arg.item.level())
			{
			case 1: // game
				db.filter.game = arg.item.text();
				db.filter.plugin.clear();
				db.filter.voicetype.clear();
				break;

			case 2: // plugin
				db.filter.plugin = arg.item.text();
				db.filter.game = arg.item.owner().text();
				db.filter.voicetype.clear();
				break;

			case 3: // voicetype
				db.filter.voicetype = arg.item.text();
				db.filter.plugin = arg.item.owner().text();
				db.filter.game = arg.item.owner().owner().text();
				break;
			}
			PopulateList();
		});

		events().mouse_up([this](const nana::arg_mouse &arg)
		{
			if(arg.button == nana::mouse::right_button && !selected().empty())
			{
				coolmenu m;

				switch(selected().level())
				{
				case 1: // game

					if(selected() != top_nodes_.front())
					{
						auto item = m.append("Move up", [this](nana::menu::item_proxy &item)
						{
							SetCursor(LoadCursor(0, IDC_WAIT));
							size_t pos{0};
							for(auto &game : db)
							{
								if(game.name() == selected().text()) break;
								else pos++;
							}

							Data::Game temp = db[selected().text()];
							db.erase(pos);
							db.insert(pos-1, temp);
							db.save(conf.db_path);
							db.rewire();
							PopulateTree();
							SetCursor(LoadCursor(0, IDC_ARROW));
						});
						nana::paint::image img_up;
						img_up.open(ico_up, sizeof ico_up);
						m.image(item.index(), img_up);
					}

					if(selected() != top_nodes_.back())
					{
						auto item = m.append("Move down", [this](nana::menu::item_proxy &item)
						{
							SetCursor(LoadCursor(0, IDC_WAIT));
							size_t pos{0};
							for(auto &game : db)
							{
								if(game.name() == selected().text()) break;
								else pos++;
							}

							Data::Game temp = db[selected().text()];
							db.erase(pos);
							db.insert(pos+1, temp);
							db.save(conf.db_path);
							db.rewire();
							PopulateTree();
							SetCursor(LoadCursor(0, IDC_ARROW));
						});
						nana::paint::image img_down;
						img_down.open(ico_down, sizeof ico_down);
						m.image(item.index(), img_down);
					}

					if(top_nodes_.size() > 1)
					{
						auto item = m.append("Remove game", [this](nana::menu::item_proxy &item)
						{
							size_t pos{0};
							for(auto &game : db)
							{
								if(game.name() == selected().text()) break;
								else pos++;
							}
							string msg{"Are you sure you want to permanently delete the data for game \"" + selected().text() +
								"\"? This change is automatically saved to the data file and cannot be undone."};
							if(MessageBoxA(hwnd, msg.data(), filepath{conf.db_path}.fullname().data(), MB_ICONEXCLAMATION|MB_YESNO) == IDYES)
							{
								SetCursor(LoadCursor(0, IDC_WAIT));
								db.erase(pos);
								db.save(conf.db_path);
								db.rewire();
								PopulateTree();
								SetCursor(LoadCursor(0, IDC_ARROW));
							}
						});
						nana::paint::image img_del;
						img_del.open(ico_del, sizeof ico_del);
						m.image(item.index(), img_del);
					}

					break;

				case 2: // plugin

					if(selected() != selected().owner().child())
					{
						auto item = m.append("Move up", [this](nana::menu::item_proxy &item)
						{
							SetCursor(LoadCursor(0, IDC_WAIT));
							auto &game = db[selected().owner().text()];
							size_t pos{0};
							for(auto &plug : game)
							{
								if(plug.name() == selected().text()) break;
								else pos++;
							}
							Data::Plugin temp = game.at(pos);
							game.erase(pos);
							game.insert(pos-1, temp);
							db.save(conf.db_path);
							db.rewire();
							PopulateTree();
							SetCursor(LoadCursor(0, IDC_ARROW));
						});
						nana::paint::image img_up;
						img_up.open(ico_up, sizeof ico_up);
						m.image(item.index(), img_up);
					}

					if(!selected().sibling().empty())
					{
						auto item = m.append("Move down", [this](nana::menu::item_proxy &item)
						{
							SetCursor(LoadCursor(0, IDC_WAIT));
							auto &game = db[selected().owner().text()];
							size_t pos{0};
							for(auto &plug : game)
							{
								if(plug.name() == selected().text()) break;
								else pos++;
							}
							Data::Plugin temp = game.at(pos);
							game.erase(pos);
							game.insert(pos+1, temp);
							db.save(conf.db_path);
							db.rewire();
							PopulateTree();
							SetCursor(LoadCursor(0, IDC_ARROW));
						});
						nana::paint::image img_down;
						img_down.open(ico_down, sizeof ico_down);
						m.image(item.index(), img_down);
					}

					if(selected().owner().size()>1)
					{
						auto item = m.append("Remove plugin", [this](nana::menu::item_proxy &item)
						{
							auto &game = db[selected().owner().text()];
							size_t pos{0};
							for(auto &plug : game)
							{
								if(plug.name() == selected().text()) break;
								else pos++;
							}
							string msg{"Are you sure you want to permanently delete the data for plugin \"" + game.at(pos).name() +
								"\"? This change is automatically saved to the data file and cannot be undone."};
							if(MessageBoxA(hwnd, msg.data(), filepath{conf.db_path}.fullname().data(), MB_ICONEXCLAMATION|MB_YESNO) == IDYES)
							{
								SetCursor(LoadCursor(0, IDC_WAIT));
								game.erase(pos);
								db.save(conf.db_path);
								db.rewire();
								PopulateTree();
								SetCursor(LoadCursor(0, IDC_ARROW));
							}
						});
						nana::paint::image img_del;
						img_del.open(ico_del, sizeof ico_del);
						m.image(item.index(), img_del);
					}
					break;

				case 3: // voice type

					if(selected().owner().size()>1)
					{
						auto item = m.append("Remove voice type", [this](nana::menu::item_proxy &item)
						{
							auto &game = db[selected().owner().owner().text()];
							auto &plug = db[game][selected().owner().text()];
							size_t pos{0};
							for(auto &vt : plug)
							{
								if(vt.name() == selected().text()) break;
								else pos++;
							}
							string msg{"Are you sure you want to permanently delete the data for voice type \"" + plug.at(pos).name() +
								"\"? This change is automatically saved to the data file and cannot be undone."};
							if(MessageBoxA(hwnd, msg.data(), filepath{conf.db_path}.fullname().data(), MB_ICONEXCLAMATION|MB_YESNO) == IDYES)
							{
								SetCursor(LoadCursor(0, IDC_WAIT));
								plug.erase(pos);
								db.save(conf.db_path);
								db.rewire();
								PopulateTree();
								SetCursor(LoadCursor(0, IDC_ARROW));
							}
						});
						nana::paint::image img_del;
						img_del.open(ico_del, sizeof ico_del);
						m.image(item.index(), img_del);
					}
				} // switch(selected().level())

				if(m.size()) m.popup_await(arg.window_handle, arg.pos.x, arg.pos.y);
			}
		});
	}

	item_proxy insert(const string &key_path, string title)
	{
		item_proxy node = treebox::insert(key_path, title);
		if(node.level() == 1) top_nodes_.push_back(node);
		return node;
	}

	bool is_top_node(string node_text)
	{
		for(const auto &top_node : top_nodes_)
			if(top_node.text() == node_text) return true;
		return false;
	}

	auto begin() { return top_nodes_.begin(); }
	auto end() { return top_nodes_.end(); }

	void clear()
	{
		for(auto &tn : top_nodes_) erase(tn);
		top_nodes_.clear();
		last_selected.clear();
	}

	bool scrollbar() { return tree->scroll_operation()->visible(true); }

} *tree = nullptr;


class coollist : public nana::listbox
{

public:

	bool display_tip = false;

	coollist(nana::window w, bool visible = true) : listbox(w, visible)
	{
		append_header("File name", 140);
		append_header("Dialogue/description");
		scheme().header_bgcolor = nana::colors::white;
		scheme().header_fgcolor = nana::color_rgb(0x995533);
		scheme().item_selected = nana::color_rgb(0xdcefe8);
		scheme().item_highlighted = nana::color_rgb(0xeaf0ef);

		column_at(0).text_align(nana::align::center);
		column_at(0).typeface({"Verdana", 11, nana::detail::font_style(1000)});
		column_at(1).typeface(column_at(0).typeface());
		typeface({"Tahoma", 10});

		events().resized([this](const nana::arg_resized &arg) { adjust_columns(); });

		nana::drawing(*this).draw([&](nana::paint::graphics &graph)
		{
			if(!display_tip || db.index.empty() || tree->selected().level() == 3) return;
			nana::point mousepos = nana::API::cursor_position();
			nana::API::calc_window_point(*this, mousepos);
			if(mousepos.x > size().width-18) return;
			index_pair hovered = cast(mousepos);
			if(hovered.item == index_pair::npos || hovered.is_category()) return;
			string text;
			if(tree->selected().level() == 1) 
				text = db.index[hovered.cat-1].index[hovered.item]->voicetype->plugin->name() + "::";
			text += db.index[hovered.cat-1].index[hovered.item]->voicetype->name();
			nana::size tsize = graph.text_extent_size(text);
			nana::rectangle r;
			r.x = mousepos.x;
			r.y = mousepos.y - 23;
			r.width = tsize.width+16;
			r.height = 23;
			if(r.x + r.width > graph.width()-17) r.x -= r.width;
			graph.round_rectangle(r, 3, 3, nana::color_rgb(0x995533), true, nana::colors::beige);
			graph.string({r.x+8, r.y+3}, text, nana::color_rgb(0x884422));
		});

		events().mouse_move([this](const nana::arg_mouse &arg)
		{
			if(display_tip && tree->selected().level() != 3)
				nana::API::refresh_window(*this);
		});

		events().mouse_leave([this] { display_tip = false; nana::API::refresh_window(*this); });
	}

	void adjust_columns()
	{
		const auto level{tree->selected().level()};
		if(level)
		{
			unsigned col0_width{140}; // 140 pixels in the case of FO4
			if(level == 3 && tree->selected().owner().owner().text() == "Skyrim SE") col0_width = 300;
			else if(level == 2 && tree->selected().owner().text() == "Skyrim SE") col0_width = 300;
			else if(level == 1 && tree->selected().text() == "Skyrim SE") col0_width = 300;
			if(col0_width == 140) column_at(0).text_align(nana::align::center);
			else column_at(0).text_align(nana::align::left);
			column_at(0).width(col0_width);
			column_at(1).width(content_area().width - col0_width - scroll_operation()->visible(true)*16);
		}
	}

} *list1(nullptr);


class progform : public nana::form
{
	bool *abort = nullptr;
	progbar prog;
	nana::button btnabort;

public:

	progform(nana::form &parent, bool &abort) : form(parent, {333, 111}, appear::decorate<>())
	{
		this->abort = &abort;
		icon(nana::paint::image(wstring(self_path)));
		bgcolor(nana::colors::white);
		caption("Progress");
		div("vert margin=16 <prog> <margin=[15,0,0,0] weight=45 <><btnabort weight=80><>>");
		events().unload([this] {*this->abort = true; });
		prog.create(handle(), true);
		btnabort.create(handle(), true);
		btnabort.caption("Abort");
		btnabort.bgcolor(nana::colors::white);
		btnabort.events().click([this] {*this->abort = true; });
		(*this)["prog"] << prog;
		(*this)["btnabort"] << btnabort;
		collocate();
	}

	unsigned amount(unsigned a) { return prog.amount(a); }
	unsigned amount() const { return prog.amount(); }
	unsigned inc() { return prog.inc(); }
	unsigned value(unsigned val) { return prog.value(val); }
	unsigned value() const { return prog.value(); }
};


class iconbox : public nana::label
{
	HICON icons[5] = {nullptr};
	int icon_{0};

public:

	enum { info, tick, time, error, save };

	iconbox(nana::window parent, int ico = info) : nana::label(parent)
	{
		icon_ = ico;
		size({32, 32});
		icons[error] = LoadIcon(NULL, IDI_ERROR);
		ExtractIconExA("%windir%\\system32\\comres.dll", 5, &icons[time], NULL, 1);
		ExtractIconExA("%windir%\\system32\\urlmon.dll", 0, &icons[tick], NULL, 1);
		ExtractIconExA("%windir%\\system32\\dmdskres.dll", 1, &icons[info], NULL, 1);
		ExtractIconExA("%windir%\\system32\\imageres.dll", 264, &icons[save], NULL, 1);

		nana::drawing(*this).draw([this](nana::paint::graphics &g)
		{
			int y = (g.height() - 32) / 2;
			DrawIconEx((HDC)g.context(), 0, y, icons[icon_], 32, 32, 0, NULL, DI_NORMAL);
		});
	}

	int icon() const noexcept { return icon_; }

	void icon(int i)
	{
		if(i < sizeof icons / sizeof(int))
		{
			icon_ = i;
			nana::API::refresh_window(*this);
		}
	}

	~iconbox() { for(auto &icon : icons) DestroyIcon(icon); }
};