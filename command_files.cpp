// File transfer: -read -type -write -export -import
#include "command.hpp"
#include <algorithm>
#include <dirent.h>
#include <map>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdio>

using namespace std;

static bool take_flag(list<string> &pars, const string &flag)
{
	for (auto it = pars.begin(); it != pars.end(); ++it)
		if (*it == flag) { pars.erase(it); return true; }
	return false;
}

// ---------------------------------------------------------------- -read

void command_read::help()
{
	cout << " -read <path> [<file>] [text]" << endl
	     << "\tCopy one DOS/50 file to a local file (default: its own name)." << endl
	     << "\t'text' turns DOS/50's CR line ends into LF." << endl;
}

void command_read::process(list<string> &pars)
{
	if (!need_volume()) return;
	bool text = take_flag(pars, "text");
	if (pars.empty() || pars.size() > 2) { error("usage: -read <path> [<file>] [text]"); return; }
	string err, path = pars.front();
	int id = vol.resolve(path, err);
	if (id < 0) { error(err); return; }
	if (vol.desc(id).is_dir()) { error(path + " is a directory (use -export)"); return; }
	auto data = vol.read_file(id);
	if (text) to_text_host(data);
	string out = pars.size() == 2 ? pars.back() : host_name(path.substr(path.find_last_of('/') + 1));
	if (!host_write(out, data, err)) { error(err); return; }
	info(path + " -> " + out + " (" + to_string(data.size()) + " bytes)");
}

// ---------------------------------------------------------------- -type

void command_type::help()
{
	cout << " -type <path>" << endl << "\tPrint a DOS/50 text file on the terminal (CR line ends shown as LF)." << endl;
}

void command_type::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.size() != 1) { error("usage: -type <path>"); return; }
	string err;
	int id = vol.resolve(pars.front(), err);
	if (id < 0) { error(err); return; }
	if (vol.desc(id).is_dir()) { error(pars.front() + " is a directory"); return; }
	auto data = vol.read_file(id);
	to_text_host(data);
	for (auto c : data) {
		if (c == '\n' || c == '\t' || (c >= 0x20 && c < 0x7f)) cout << char(c);
		else { char b[8]; snprintf(b, sizeof b, "<%02X>", c); cout << b; }
	}
	if (!data.empty() && data.back() != '\n') cout << endl;
}

// ---------------------------------------------------------------- -write

void command_write::help()
{
	cout << " -write <file> [<path>] [text]" << endl
	     << "\tCopy a local file onto the volume (default: root, its own name)." << endl
	     << "\tA path ending in / names a directory. An existing file is" << endl
	     << "\treplaced. 'text' turns LF / CR LF into DOS/50's CR." << endl;
}

static bool put_file(dos50::volume &vol, const string &src, string dest, bool text, string &err, const string &owner_name,
                     bool (*rd)(const string &, vector<dos50::u8> &, string &), void (*conv)(vector<dos50::u8> &), string (*mkname)(const string &))
{
	vector<dos50::u8> data;
	if (!rd(src, data, err)) return false;
	if (text) conv(data);
	if (dest.empty() || dest.back() == '/') dest += mkname(src);
	string leaf;
	int dir = vol.resolve_parent(dest, leaf, err);
	if (dir < 0) return false;
	dos50::u32 now = dos50::volume::now_local();
	for (auto &e : vol.list_dir(dir))
		if (e.name == leaf) {
			if (!vol.replace_data(e.id, data, now, err)) return false;
			return true;
		}
	return vol.create_file(dir, leaf, data, now, owner_name, err) >= 0;
}

void command_write::process(list<string> &pars)
{
	if (!need_volume()) return;
	bool text = take_flag(pars, "text");
	if (pars.empty() || pars.size() > 2) { error("usage: -write <file> [<path>] [text]"); return; }
	string err, dest = pars.size() == 2 ? pars.back() : "";
	if (!put_file(vol, pars.front(), dest, text, err, owner, host_read, to_text_dos50, dos50_name)) { error(err); return; }
	dirty = true;
	info(pars.front() + " -> " + (dest.empty() || dest.back() == '/' ? dest + dos50_name(pars.front()) : dest));
}

// ---------------------------------------------------------------- -export

void command_export::help()
{
	cout << " -export [<directory>] [<path>] [text]" << endl
	     << "\tCopy a DOS/50 directory tree (default: the whole volume) into a" << endl
	     << "\tlocal directory (default: current). Names keep their abbreviation" << endl
	     << "\tbrackets (ATT[RIBUTE]). 'text' converts every file's CR to LF." << endl;
}

static bool mkdir_p(const string &d)
{
	struct stat st;
	if (stat(d.c_str(), &st) == 0) return S_ISDIR(st.st_mode);
	return ::mkdir(d.c_str(), 0755) == 0;
}

void command_export::process(list<string> &pars)
{
	if (!need_volume()) return;
	bool text = take_flag(pars, "text");
	string out = ".", path;
	if (!pars.empty()) { out = pars.front(); pars.pop_front(); }
	if (!pars.empty()) { path = pars.front(); pars.pop_front(); }
	string err;
	int root = vol.resolve(path, err);
	if (root < 0) { error(err); return; }
	if (!vol.desc(root).is_dir()) { error(path + " is not a directory"); return; }
	int files = 0, links = 0;
	map<int, string> first;                  // file id -> first host path (DOS/50 links become host links)
	struct job { int id; string dir; int depth; };
	vector<job> todo{ { root, out, 0 } };
	while (!todo.empty()) {
		job j = todo.back(); todo.pop_back();
		if (!mkdir_p(j.dir)) { error("cannot create " + j.dir); return; }
		if (j.depth > 16) { error("directory nesting too deep at " + j.dir); continue; }
		for (auto &e : vol.list_dir(j.id)) {
			string target = j.dir + "/" + host_name(e.name);
			if (vol.desc(e.id).is_dir()) { todo.push_back({ e.id, target, j.depth + 1 }); continue; }
			auto f = first.find(e.id);
			if (f != first.end()) {
				remove(target.c_str());
				if (::link(f->second.c_str(), target.c_str()) == 0) { links++; continue; }
			}
			auto data = vol.read_file(e.id);
			if (text) to_text_host(data);
			if (!host_write(target, data, err)) { error(err); continue; }
			first.emplace(e.id, target);
			files++;
		}
	}
	info(to_string(files) + " files exported to " + out + (links ? ", " + to_string(links) + " more names as hard links" : ""));
}

// ---------------------------------------------------------------- -import

void command_import::help()
{
	cout << " -import <directory> [<path>] [text]" << endl
	     << "\tCopy every file of a local directory (with its subdirectories)" << endl
	     << "\tinto a DOS/50 directory (default: root). Names are cut to 14" << endl
	     << "\tcharacters; existing files are replaced." << endl;
}

void command_import::process(list<string> &pars)
{
	if (!need_volume()) return;
	bool text = take_flag(pars, "text");
	if (pars.empty() || pars.size() > 2) { error("usage: -import <directory> [<path>] [text]"); return; }
	string err, base = pars.front(), dest = pars.size() == 2 ? pars.back() : "";
	int files = 0, links = 0;
	map<pair<dev_t, ino_t>, int> seen;      // host hard links become DOS/50 links
	struct job { string host, dos; int depth; };
	vector<job> todo{ { base, dest, 0 } };
	while (!todo.empty()) {
		job j = todo.back(); todo.pop_back();
		DIR *d = opendir(j.host.c_str());
		if (!d) { error("cannot read directory " + j.host); continue; }
		vector<string> names;
		while (dirent *de = readdir(d)) if (de->d_name[0] != '.') names.push_back(de->d_name);
		closedir(d);
		sort(names.begin(), names.end());
		for (auto &n : names) {
			string hp = j.host + "/" + n, dn = dos50_name(n);
			string dp = j.dos.empty() ? dn : j.dos + "/" + dn;
			struct stat st;
			if (stat(hp.c_str(), &st) != 0) continue;
			if (S_ISDIR(st.st_mode)) {
				if (j.depth >= 16) { error("too deep: " + hp); continue; }
				string leaf;
				int parent = vol.resolve_parent(dp, leaf, err);
				if (parent < 0) { error(err); continue; }
				int existing = vol.find(parent, leaf);
				if (existing < 0 && vol.make_dir(parent, leaf, dos50::volume::now_local(), owner, err) < 0) { error(err); continue; }
				todo.push_back({ hp, dp, j.depth + 1 });
			} else if (S_ISREG(st.st_mode)) {
				auto key = make_pair(st.st_dev, st.st_ino);
				auto s = st.st_nlink > 1 ? seen.find(key) : seen.end();
				if (s != seen.end()) {
					string leaf;
					int parent = vol.resolve_parent(dp, leaf, err);
					if (parent >= 0 && vol.find(parent, leaf) < 0 && vol.link(s->second, parent, leaf, err)) { links++; continue; }
				}
				if (!put_file(vol, hp, dp, text, err, owner, host_read, to_text_dos50, dos50_name)) { error(hp + ": " + err); continue; }
				if (st.st_nlink > 1) { int id = vol.resolve(dp, err); if (id > 0) seen.emplace(key, id); }
				files++;
			}
		}
	}
	dirty = true;
	info(to_string(files) + " files imported from " + base + (links ? ", " + to_string(links) + " more names as links" : ""));
}
