// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// File transfer: -read -type -write -export -import
#include "command.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <system_error>

using namespace std;
namespace hfs = std::filesystem;           // the host's file system (portable: POSIX and Windows)

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
	const hfs::path out = pars.size() == 2 ? hfs::path(pars.back()) : host_file(fs->host_name(path.substr(path.find_last_of('/') + 1)));
	if (!host_write(out, data, err)) { error(err); return; }
	info(path + " -> " + out.u8string() + " (" + to_string(data.size()) + " bytes)");
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
	info(pars.front() + " -> " + (dest.empty() || dest.back() == '/' ? dest + disc_name_for(pars.front()) : dest));
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

static bool mkdir_p(const hfs::path &d)
{
	error_code ec;
	if (hfs::is_directory(d, ec)) return true;
	return hfs::create_directories(d, ec) || hfs::is_directory(d, ec);
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
	map<int, hfs::path> first;               // file id -> first host path (links become host links)
	struct job { int id; hfs::path dir; int depth; };
	vector<job> todo{ { top, hfs::path(out), 0 } };
	while (!todo.empty()) {
		job j = todo.back(); todo.pop_back();
		if (!mkdir_p(j.dir)) { error("cannot create " + j.dir.u8string()); return; }
		if (j.depth > 16) { error("directory nesting too deep at " + j.dir.u8string()); continue; }
		for (auto &e : fs->list_dir(j.id)) {
			const hfs::path target = j.dir / host_file(fs->host_name(e.name));
			if (e.dir) { todo.push_back({ e.id, target, j.depth + 1 }); continue; }
			auto f = first.find(e.id);
			if (f != first.end()) {
				// a host hard link where the host file system has them (not FAT/exFAT): else a copy
				error_code ec;
				hfs::remove(target, ec);
				hfs::create_hard_link(f->second, target, ec);
				if (!ec) { links++; continue; }
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
	vector<pair<hfs::path, int>> seen;      // host hard links become links on the volume
	struct job { hfs::path host; string path; int depth; };
	vector<job> todo{ { hfs::path(base), dest, 0 } };
	while (!todo.empty()) {
		job j = todo.back(); todo.pop_back();
		error_code ec;
		vector<hfs::path> names;
		for (hfs::directory_iterator it(j.host, ec), end; !ec && it != end; it.increment(ec))
			if (it->path().filename().u8string()[0] != '.') names.push_back(it->path());
		if (ec) { error("cannot read directory " + j.host.u8string()); continue; }
		sort(names.begin(), names.end());
		for (auto &hp : names) {
			const string vn = disc_name_for(hp);
			const string vp = j.path.empty() ? vn : j.path + "/" + vn;
			const auto st = hfs::status(hp, ec);
			if (ec) continue;
			string leaf;
			if (hfs::is_directory(st)) {
				if (j.depth >= 16) { error("too deep: " + hp.u8string()); continue; }
				int parent = fs->resolve_parent(vp, leaf, err);
				if (parent < 0) { error(err); continue; }
				if (fs->find(parent, leaf) < 0 && fs->make_dir(parent, leaf, err) < 0) { cout << "W : " << hp.u8string() << ": skipped (" << err << ")" << endl; continue; }
				todo.push_back({ hp, vp, j.depth + 1 });
			} else if (hfs::is_regular_file(st)) {
				const bool multi = hfs::hard_link_count(hp, ec) > 1 && !ec;
				if (multi) {
					auto s = find_if(seen.begin(), seen.end(), [&](const pair<hfs::path, int> &p) { error_code e; return hfs::equivalent(p.first, hp, e); });
					if (s != seen.end()) {
						int parent = fs->resolve_parent(vp, leaf, err);
						string lerr;
						if (parent >= 0 && fs->find(parent, leaf) < 0 && fs->link(s->second, parent, leaf, lerr)) { links++; continue; }
					}
				}
				int id = -1;
				if (!put_file(hp, vp, text, err, &id)) { error(hp.u8string() + ": " + err); continue; }
				if (multi) seen.emplace_back(hp, id);
				files++;
			}
		}
	}
	dirty = true;
	info(to_string(files) + " files imported from " + base + (links ? ", " + to_string(links) + " more names as links" : ""));
}
