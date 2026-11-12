// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// -list: the directory as the open system shows it
#include "command.hpp"

using namespace std;

void command_list::help()
{
	cout << " -list [<directory>] [flat] [ids]" << endl
	     << "\tList a directory tree (default: the whole volume)." << endl
	     << "\tDOS/50 columns: size in bytes, blocks used, links, capabilities," << endl
	     << "\towner and last update. Names keep DOS/50's abbreviation" << endl
	     << "\tbrackets: ATT[RIBUTE] answers to ATT, ATTR, ... ATTRIBUTE." << endl
	     << "\t  flat  only the named directory, no subdirectories" << endl
	     << "\t  ids   also show file ids (descriptor numbers)" << endl
	     << "\tTEKDOS: as its LDIR / shows it (blocks, sectors, bytes in the last" << endl
	     << "\tsector, and a load module's IDENT, LOAD and START), with LDIR's totals." << endl;
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
	int id = fs->resolve(path, err);
	if (id < 0) { error(err); return; }
	if (!fs->is_dir(id)) { error(path + " is not a directory"); return; }
	cout << "  Volume " << fs->volume_name() << (path.empty() ? "" : ", " + path) << endl;
	fs->print_list(cout, id, recurse, ids);
}
