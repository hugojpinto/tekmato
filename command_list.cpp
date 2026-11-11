// -list: directory tree with DOS/50 attributes
#include "command.hpp"
#include <iomanip>
#include <algorithm>
#include <set>

using namespace std;

void command_list::help()
{
	cout << " -list [<directory>] [flat] [ids]" << endl
	     << "\tList a directory tree (default: the whole volume)." << endl
	     << "\tColumns: size in bytes, blocks used, links, capabilities," << endl
	     << "\towner and last update. Names keep DOS/50's abbreviation" << endl
	     << "\tbrackets: ATT[RIBUTE] answers to ATT, ATTR, ... ATTRIBUTE." << endl
	     << "\t  flat  only the named directory, no subdirectories" << endl
	     << "\t  ids   also show file ids (descriptor numbers)" << endl;
}

static void list_dir(dos50::volume &vol, int dir, const string &indent, bool recurse, bool ids, set<int> &open, int &files, int &dirs)
{
	open.insert(dir);
	for (auto &e : vol.list_dir(dir)) {
		dos50::descriptor d = vol.desc(e.id);
		string name = e.name + (d.is_dir() ? "/" : "");
		cout << "  " << indent << left << setw(max(1, 18 - int(indent.size()))) << name << right;
		if (d.is_dir()) cout << setw(8) << "<DIR>";
		else cout << setw(8) << d.size();
		cout << setw(5) << d.consumption() << setw(3) << int(d.links()) << "  " << left << setw(12) << dos50::volume::caps_string(d.caps())
		     << setw(10) << d.owner() << right << dos50::volume::format_time(d.mtime());
		if (ids) cout << "  #" << e.id;
		cout << endl;
		if (d.is_dir()) dirs++; else files++;
		if (d.is_dir() && recurse && !open.count(e.id)) list_dir(vol, e.id, indent + "  ", recurse, ids, open, files, dirs);
	}
	open.erase(dir);
}

void command_list::process(list<string> &pars)
{
	if (!need_volume()) return;
	string path;
	bool recurse = true, ids = false;
	for (auto &p : pars) {
		if (p == "flat") recurse = false;
		else if (p == "ids") ids = true;
		else path = p;
	}
	string err;
	int id = vol.resolve(path, err);
	if (id < 0) { error(err); return; }
	if (!vol.desc(id).is_dir()) { error(path + " is not a directory"); return; }
	cout << "  Volume " << vol.volume_name() << (path.empty() ? "" : ", " + path) << endl;
	cout << "  " << left << setw(18) << "NAME" << right << setw(8) << "SIZE" << setw(5) << "BLK" << setw(3) << "LK"
	     << "  " << left << setw(12) << "CAP" << setw(10) << "OWNER" << right << "UPDATED" << endl;
	set<int> open;
	int files = 0, dirs = 0;
	list_dir(vol, id, "", recurse, ids, open, files, dirs);
	cout << "  " << files << " files, " << dirs << " directories; "
	     << vol.maxfiles() - vol.free_files() << " files used, " << vol.free_files() << " free files, "
	     << vol.free_blocks() << " free blocks" << endl;
}
