// Directory operations: -delete -mkdir -link -move -attrib -check
#include "command.hpp"
#include <set>

using namespace std;

// ---------------------------------------------------------------- -delete

void command_delete::help()
{
	cout << " -delete <path> [<path> ...]" << endl
	     << "\tRemove directory entries, as DOS/50 DELETE does: a file is freed" << endl
	     << "\twhen its last link goes. A directory is removed with everything" << endl
	     << "\tin it (files still linked elsewhere survive)." << endl;
}

static bool delete_tree(dos50::volume &vol, int dir, const string &name, string &err, int depth)
{
	int id = vol.find(dir, name);
	if (id < 0) { err = "file not found: " + name; return false; }
	// exact stored name (find() also accepts abbreviations)
	string stored;
	for (auto &e : vol.list_dir(dir)) if (e.id == id && dos50::volume::name_matches(e.name, name)) { stored = e.name; break; }
	if (vol.desc(id).is_dir() && vol.desc(id).links() <= 1) {
		if (depth > 16) { err = "directory nesting too deep"; return false; }
		for (auto &e : vol.list_dir(id))
			if (!delete_tree(vol, id, e.name, err, depth + 1)) return false;
	}
	return vol.unlink(dir, stored, err);
}

void command_delete::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.empty()) { error("usage: -delete <path> ..."); return; }
	for (auto &p : pars) {
		string err, leaf;
		int dir = vol.resolve_parent(p, leaf, err);
		if (dir < 0 || !delete_tree(vol, dir, leaf, err, 0)) { error(err); continue; }
		dirty = true;
		info("deleted " + p);
	}
}

// ---------------------------------------------------------------- -mkdir

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
		int dir = vol.resolve_parent(p, leaf, err);
		if (dir < 0 || vol.make_dir(dir, leaf, dos50::volume::now_local(), owner, err) < 0) { error(err); continue; }
		dirty = true;
		info("created " + p);
	}
}

// ---------------------------------------------------------------- -link

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
	string err, leaf;
	int id = vol.resolve(pars.front(), err);
	if (id < 0) { error(err); return; }
	if (vol.desc(id).is_dir()) { error("directories cannot be linked"); return; }
	string dest = pars.back();
	if (dest.back() == '/') dest += pars.front().substr(pars.front().find_last_of('/') + 1);
	int dir = vol.resolve_parent(dest, leaf, err);
	if (dir < 0 || !vol.link(id, dir, leaf, err)) { error(err); return; }
	dirty = true;
	info("linked " + pars.front() + " as " + dest);
}

// ---------------------------------------------------------------- -move

void command_move::help()
{
	cout << " -move <path> <newpath>" << endl
	     << "\tRename a file or directory, or move it to another directory" << endl
	     << "\t(DOS/50 RENAME; a path ending in / keeps the name)." << endl;
}

void command_move::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.size() != 2) { error("usage: -move <path> <newpath>"); return; }
	string err, leaf, nleaf, src = pars.front(), dest = pars.back();
	int dir = vol.resolve_parent(src, leaf, err);
	if (dir < 0) { error(err); return; }
	int id = vol.find(dir, leaf);
	if (id < 0) { error("file not found: " + src); return; }
	for (auto &e : vol.list_dir(dir)) if (e.id == id && dos50::volume::name_matches(e.name, leaf)) { leaf = e.name; break; }
	if (dest.back() == '/') dest += leaf;
	int ndir = vol.resolve_parent(dest, nleaf, err);
	if (ndir < 0) { error(err); return; }
	if (ndir == dir) { if (!vol.rename_entry(dir, leaf, nleaf, err)) { error(err); return; } }
	else {
		if (vol.desc(id).is_dir()) {
			// refuse to move a directory into itself
			for (int p = ndir, guard = 0; guard < 64; guard++) {
				if (p == id) { error("cannot move a directory into itself"); return; }
				bool up = false;
				// walk up: find the directory that names p
				for (int cand = 0; cand < vol.maxfiles() && !up; cand++)
					if (vol.id_in_use(cand) && vol.desc(cand).is_dir())
						for (auto &e : vol.list_dir(cand)) if (e.id == p && cand != p) { p = cand; up = true; break; }
				if (!up || p == 0) break;
			}
		}
		// link under the new name, then drop the old entry (the descriptor stays)
		// (while linked twice, unlink() only drops the entry and the link count)
		if (!vol.link(id, ndir, nleaf, err) || !vol.unlink(dir, leaf, err)) { error(err); return; }
	}
	dirty = true;
	info("moved " + src + " to " + dest);
}

// ---------------------------------------------------------------- -attrib

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
	int id = vol.resolve(path, err);
	if (id < 0) { error(err); return; }
	dos50::descriptor d = vol.desc(id);
	for (auto &p : pars) {
		if (p.compare(0, 4, "cap=") == 0 || p.compare(0, 4, "CAP=") == 0) {
			dos50::u8 caps = 0;
			string v = p.substr(4), tok;
			v += "/";
			for (char c : v) {
				if (c != '/') { tok += char(toupper(c)); continue; }
				if (tok == "OR") caps |= dos50::CAP_OR;
				else if (tok == "OW") caps |= dos50::CAP_OW;
				else if (tok == "PR") caps |= dos50::CAP_PR;
				else if (tok == "PW") caps |= dos50::CAP_PW;
				else if (!tok.empty()) { error("unknown capability " + tok + " (OR, OW, PR, PW)"); return; }
				tok.clear();
			}
			d.set_caps(caps);
			dirty = true;
		} else if (p.compare(0, 6, "owner=") == 0 || p.compare(0, 6, "OWNER=") == 0) {
			if (p.size() - 6 > 14) { error("owner names have at most 14 characters"); return; }
			d.set_owner(p.substr(6));
			dirty = true;
		} else { error("unknown attribute " + p); return; }
	}
	cout << "  " << path << endl
	     << "    CAPABILITIES=" << dos50::volume::caps_string(d.caps()) << endl
	     << "    CREATION=" << dos50::volume::format_time(d.ctime()) << endl
	     << "    UPDATE=" << dos50::volume::format_time(d.mtime()) << endl
	     << "    ACCESS=" << dos50::volume::format_time(d.atime()) << "  OWNER=" << d.owner() << endl
	     << "    SIZE=" << d.size() << "  CONSUMPTION=" << d.consumption() << "  LINK=" << int(d.links())
	     << "  TYPE=" << (d.is_dir() ? "DIR" : "FILE") << endl
	     << "    ID=" << id << "  EXTENTS=";
	for (auto &e : d.extents()) cout << e.block << "+" << e.count << " ";
	cout << endl;
}

// ---------------------------------------------------------------- -check

void command_check::help()
{
	cout << " -check [fix]" << endl
	     << "\tVerify the volume: bitmaps against descriptors, link counts," << endl
	     << "\tshared blocks. Lists orphans (descriptors in use that no directory" << endl
	     << "\tnames; DOS/50 leaves these behind). 'fix' frees the orphans." << endl;
}

void command_check::process(list<string> &pars)
{
	if (!need_volume()) return;
	bool fix = !pars.empty() && pars.front() == "fix";
	auto r = vol.check();
	for (auto &p : r.problems) cout << "W : " << p << endl;
	if (!r.orphans.empty()) {
		cout << "I : " << r.orphans.size() << " orphan files holding " << r.orphan_blocks << " blocks:";
		for (int id : r.orphans) cout << " #" << id;
		cout << endl;
	}
	if (r.problems.empty() && r.orphans.empty()) info("volume is consistent");
	if (fix && !r.orphans.empty()) {
		int n = vol.free_orphans(r);
		dirty = true;
		info("freed " + to_string(n) + " orphans; " + to_string(vol.free_blocks()) + " free blocks, " + to_string(vol.free_files()) + " free files");
	}
}
