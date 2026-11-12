// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Directory operations: -delete -mkdir -link -move -attrib -check
#include "command.hpp"

using namespace std;

void command_delete::help()
{
	cout << " -delete <path> [<path> ...]" << endl
	     << "\tRemove entries, as the system's DELETE does. DOS/50: a file is freed" << endl
	     << "\twhen its last link goes; a directory is removed with everything in it" << endl
	     << "\t(files still linked elsewhere survive)." << endl;
}

void command_delete::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.empty()) { error("usage: -delete <path> ..."); return; }
	for (auto &p : pars) {
		string err, leaf;
		int dir = fs->resolve_parent(p, leaf, err);
		if (dir < 0 || !fs->remove(dir, leaf, err)) { error(err); continue; }
		dirty = true;
		info("deleted " + p);
	}
}

void command_mkdir::help()
{
	cout << " -mkdir <path> [<path> ...]" << endl << "\tCreate directories (DOS/50 CREATE)." << endl;
}

void command_mkdir::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.empty()) { error("usage: -mkdir <path> ..."); return; }
	for (auto &p : pars) {
		string err, leaf;
		int dir = fs->resolve_parent(p, leaf, err);
		if (dir < 0 || fs->make_dir(dir, leaf, err) < 0) { error(err); continue; }
		dirty = true;
		info("created " + p);
	}
}

void command_link::help()
{
	cout << " -link <path> <newpath>" << endl
	     << "\tAdd another directory entry for an existing file (DOS/50 FLINK)." << endl
	     << "\tBoth names then share one file; its link count goes up." << endl;
}

void command_link::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.size() != 2) { error("usage: -link <path> <newpath>"); return; }
	string err, leaf, dest = pars.back();
	int id = fs->resolve(pars.front(), err);
	if (id < 0) { error(err); return; }
	if (dest.back() == '/') dest += pars.front().substr(pars.front().find_last_of('/') + 1);
	int dir = fs->resolve_parent(dest, leaf, err);
	if (dir < 0 || !fs->link(id, dir, leaf, err)) { error(err); return; }
	dirty = true;
	info("linked " + pars.front() + " as " + dest);
}

void command_move::help()
{
	cout << " -move <path> <newpath>" << endl
	     << "\tRename a file or directory, or move it to another directory" << endl
	     << "\t(RENAME; a path ending in / keeps the name)." << endl;
}

void command_move::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.size() != 2) { error("usage: -move <path> <newpath>"); return; }
	string err, leaf, nleaf, src = pars.front(), dest = pars.back();
	int dir = fs->resolve_parent(src, leaf, err);
	if (dir < 0) { error(err); return; }
	if (dest.back() == '/') dest += leaf;
	int ndir = fs->resolve_parent(dest, nleaf, err);
	if (ndir < 0 || !fs->rename(dir, leaf, ndir, nleaf, err)) { error(err); return; }
	dirty = true;
	info("moved " + src + " to " + dest);
}

void command_attrib::help()
{
	cout << " -attrib <path> [cap=<OR/OW/PR/PW>] [owner=<name>]" << endl
	     << "\tShow a file's attributes as DOS/50 ATTRIBUTE does, or change its" << endl
	     << "\tcapabilities (owner/public read and write) or owner." << endl;
}

void command_attrib::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.empty()) { error("usage: -attrib <path> [cap=...] [owner=...]"); return; }
	string err, path = pars.front();
	pars.pop_front();
	int id = fs->resolve(path, err);
	if (id < 0 || !fs->attributes(id, path, pars, cout, err)) { error(err); return; }
	if (!pars.empty()) dirty = true;
}

void command_check::help()
{
	cout << " -check [fix]" << endl
	     << "\tVerify the volume. DOS/50: bitmaps against descriptors, link counts," << endl
	     << "\tshared blocks; lists orphans (descriptors in use that no directory" << endl
	     << "\tnames; DOS/50 leaves these behind), and 'fix' frees them. TEKDOS:" << endl
	     << "\tthe master map against the files' maps and the bad-block map, shared" << endl
	     << "\tblocks, each file's sectors, and the file count." << endl;
}

void command_check::process(list<string> &pars)
{
	if (!need_volume()) return;
	const bool fix = !pars.empty() && pars.front() == "fix";
	if (fs->check(cout, fix)) info("volume is consistent");
	if (fix) dirty = true;
}
