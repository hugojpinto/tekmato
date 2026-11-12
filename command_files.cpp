// File transfer: -read -type -write -export -import
#include "command.hpp"
#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <map>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

// ---------------------------------------------------------------- -read

void command_read::help()
{
	cout << " -read <path> [<file>] [text]" << endl
	     << "\tCopy one file to a local file (default: its own name)." << endl
	     << "\t'text' turns CR line ends into LF." << endl;
}

void command_read::process(list<string> &pars)
{
	if (!need_volume()) return;
	bool text = take_flag(pars, "text");
	if (pars.empty() || pars.size() > 2) { error("usage: -read <path> [<file>] [text]"); return; }
	string err, path = pars.front();
	int id = fs->resolve(path, err);
	if (id < 0) { error(err); return; }
	if (fs->is_dir(id)) { error(path + " is a directory (use -export)"); return; }
	auto data = fs->read_file(id);
	if (text) text_to_host(data);
	string out = pars.size() == 2 ? pars.back() : fs->host_name(path.substr(path.find_last_of('/') + 1));
	if (!host_write(out, data, err)) { error(err); return; }
	info(path + " -> " + out + " (" + to_string(data.size()) + " bytes)");
}

// ---------------------------------------------------------------- -type

void command_type::help()
{
	cout << " -type <path>" << endl << "\tPrint a text file on the terminal (CR line ends shown as LF)." << endl;
}

void command_type::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.size() != 1) { error("usage: -type <path>"); return; }
	string err;
	int id = fs->resolve(pars.front(), err);
	if (id < 0) { error(err); return; }
	if (fs->is_dir(id)) { error(pars.front() + " is a directory"); return; }
	auto data = fs->read_file(id);
	text_to_host(data);
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
	     << "\tCopy a local file onto the volume (default: root, a legal name made" << endl
	     << "\tfrom its own: DOS/50 keeps case and cuts to 14; TEKDOS makes it upper" << endl
	     << "\tcase, 8 characters, '.' as ';', so hello.asm -> HELLO;AS). A path" << endl
	     << "\tending in / names a directory. An existing file is replaced. 'text'" << endl
	     << "\tturns LF / CR LF into CR." << endl;
}

void command_write::process(list<string> &pars)
{
	if (!need_volume()) return;
	bool text = take_flag(pars, "text");
	if (pars.empty() || pars.size() > 2) { error("usage: -write <file> [<path>] [text]"); return; }
	string err, dest = pars.size() == 2 ? pars.back() : "";
	if (!put_file(pars.front(), dest, text, err)) { error(err); return; }
	dirty = true;
	info(pars.front() + " -> " + (dest.empty() || dest.back() == '/' ? dest + fs->name_from_host(pars.front()) : dest));
}

// ---------------------------------------------------------------- -export

void command_export::help()
{
	cout << " -export [<directory>] [<path>] [text]" << endl
	     << "\tCopy a directory tree (default: the whole volume) into a local" << endl
	     << "\tdirectory (default: current). Links become host hard links. DOS/50" << endl
	     << "\tnames keep their abbreviation brackets (ATT[RIBUTE]); TEKDOS's system" << endl
	     << "\toverlay file '.' becomes SYSTEM.SOF. 'text' converts CR to LF." << endl;
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
	int top = fs->resolve(path, err);
	if (top < 0) { error(err); return; }
	if (!fs->is_dir(top)) { error(path + " is not a directory"); return; }
	int files = 0, links = 0;
	map<int, string> first;                  // file id -> first host path (links become host links)
	struct job { int id; string dir; int depth; };
	vector<job> todo{ { top, out, 0 } };
	while (!todo.empty()) {
		job j = todo.back(); todo.pop_back();
		if (!mkdir_p(j.dir)) { error("cannot create " + j.dir); return; }
		if (j.depth > 16) { error("directory nesting too deep at " + j.dir); continue; }
		for (auto &e : fs->list_dir(j.id)) {
			string target = j.dir + "/" + fs->host_name(e.name);
			if (e.dir) { todo.push_back({ e.id, target, j.depth + 1 }); continue; }
			auto f = first.find(e.id);
			if (f != first.end()) {
				remove(target.c_str());
				if (::link(f->second.c_str(), target.c_str()) == 0) { links++; continue; }
			}
			auto data = fs->read_file(e.id);
			if (text) text_to_host(data);
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
	     << "\tCopy every file of a local directory, with its subdirectories, into a" << endl
	     << "\tdirectory of the volume (default: root), named as -write names them;" << endl
	     << "\texisting files are replaced. Host hard links become links where the" << endl
	     << "\tsystem has them. TEKDOS: SYSTEM.SOF goes back to '.'; subdirectories" << endl
	     << "\tare skipped." << endl;
}

void command_import::process(list<string> &pars)
{
	if (!need_volume()) return;
	bool text = take_flag(pars, "text");
	if (pars.empty() || pars.size() > 2) { error("usage: -import <directory> [<path>] [text]"); return; }
	string err, base = pars.front(), dest = pars.size() == 2 ? pars.back() : "";
	int files = 0, links = 0;
	map<pair<dev_t, ino_t>, int> seen;      // host hard links become links on the volume
	struct job { string host, path; int depth; };
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
			const string hp = j.host + "/" + n, vn = fs->name_from_host(n);
			const string vp = j.path.empty() ? vn : j.path + "/" + vn;
			struct stat st;
			if (stat(hp.c_str(), &st) != 0) continue;
			string leaf;
			if (S_ISDIR(st.st_mode)) {
				if (j.depth >= 16) { error("too deep: " + hp); continue; }
				int parent = fs->resolve_parent(vp, leaf, err);
				if (parent < 0) { error(err); continue; }
				if (fs->find(parent, leaf) < 0 && fs->make_dir(parent, leaf, err) < 0) { cout << "W : " << hp << ": skipped (" << err << ")" << endl; continue; }
				todo.push_back({ hp, vp, j.depth + 1 });
			} else if (S_ISREG(st.st_mode)) {
				auto key = make_pair(st.st_dev, st.st_ino);
				auto s = st.st_nlink > 1 ? seen.find(key) : seen.end();
				if (s != seen.end()) {
					int parent = fs->resolve_parent(vp, leaf, err);
					string lerr;
					if (parent >= 0 && fs->find(parent, leaf) < 0 && fs->link(s->second, parent, leaf, lerr)) { links++; continue; }
				}
				int id = -1;
				if (!put_file(hp, vp, text, err, &id)) { error(hp + ": " + err); continue; }
				if (st.st_nlink > 1) seen.emplace(key, id);
				files++;
			}
		}
	}
	dirty = true;
	info(to_string(files) + " files imported from " + base + (links ? ", " + to_string(links) + " more names as links" : ""));
}
