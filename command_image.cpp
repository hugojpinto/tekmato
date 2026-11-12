// Image-level commands: -open -new -save -saveas -info -rename -owner
#include "command.hpp"
#include <cctype>

using namespace std;

void command_open::help()
{
	cout << " -open <file>" << endl
	     << "\tOpen a volume. Its format is found from the file: a raw DOS/50 image" << endl
	     << "\t(any of the 8501's four formats) or ImageDisk (.imd) dump, or a raw" << endl
	     << "\tTEKDOS image (315,392 bytes)." << endl;
}

void command_open::process(list<string> &pars)
{
	if (pars.size() != 1) { error("usage: -open <file>"); return; }
	string err;
	auto v = tekmato::open_image(pars.front(), owner, err);
	if (!v) { error(err); return; }
	fs = std::move(v);
	dirty = false;
	info(fs->summary());
}

void command_new::help()
{
	cout << " -new [<maxfiles>] [<volume>] [<format>]" << endl
	     << "\tCreate an empty volume in memory, laid out exactly as the system's own" << endl
	     << "\tFORMAT does, and follow with -saveas. Formats (default dos50-dsdd):" << endl;
	for (auto &f : tekmato::formats()) cout << "\t  " << f.name << "  " << f.description << endl;
	cout << "\tDOS/50: <maxfiles> default 256, <volume> up to 14 characters (default" << endl
	     << "\tTEKMATO); single density is FORMAT:S. Not bootable. The root belongs to" << endl
	     << "\tthe -owner user (default NO.NAME, DOS/50's default user)." << endl
	     << "\tTEKDOS: <volume> is the disc name (48 characters); always 78 files;" << endl
	     << "\ttracks 1-4 are reserved for TEKDOS, as FORMAT leaves them." << endl;
}

void command_new::process(list<string> &pars)
{
	tekmato::new_volume v;
	v.name = "TEKMATO"; v.owner = owner;
	const tekmato::format *f = tekmato::find_format("dos50-dsdd");
	bool have_name = false;
	for (auto &p : pars) {
		if (const tekmato::format *g = tekmato::find_format(p)) { f = g; continue; }
		if (v.maxfiles < 0 && !p.empty() && isdigit(static_cast<unsigned char>(p[0]))) v.maxfiles = atoi(p.c_str());
		else if (!have_name) { v.name = p; have_name = true; }
		else {
			string names;
			for (auto &g : tekmato::formats()) names += string(names.empty() ? "" : "|") + g.name;
			error("usage: -new [<maxfiles>] [<volume>] [" + names + "]"); return;
		}
	}
	string err;
	auto nv = f->create(f->name, v, err);
	if (!nv) { error(err); return; }
	fs = std::move(nv);
	dirty = true;
	info("new " + string(f->name) + " volume " + v.name);
}

void command_save::help()
{
	cout << " -save" << endl << "\tWrite the volume back to the file it was opened from." << endl;
}

void command_save::process(list<string> &)
{
	if (!need_volume()) return;
	const string path = fs->path();
	if (path.empty()) { error("no file name yet: use -saveas"); return; }
	string err;
	if (!fs->save(path, err)) { error(err); return; }
	dirty = false;
	info("saved " + path);
}

void command_saveas::help()
{
	cout << " -saveas <file>" << endl
	     << "\tWrite the volume to a new file: .imd writes ImageDisk in the 8501's" << endl
	     << "\tphysical format, anything else a raw linear image (TEKDOS: raw only)." << endl;
}

void command_saveas::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.size() != 1) { error("usage: -saveas <file>"); return; }
	string err;
	if (!fs->save(pars.front(), err)) { error(err); return; }
	dirty = false;
	info("saved " + pars.front());
}

void command_info::help()
{
	cout << " -info" << endl << "\tShow the format, volume label, layout and free space." << endl;
}

void command_info::process(list<string> &)
{
	if (need_volume()) fs->print_info(cout);
}

void command_rename::help()
{
	cout << " -rename <volume>" << endl << "\tChange the volume name (DOS/50: 14 characters; TEKDOS disc name: 48)." << endl;
}

void command_rename::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.size() != 1) { error("usage: -rename <volume>"); return; }
	string err;
	if (!fs->set_volume_name(pars.front(), err)) { error(err); return; }
	dirty = true;
}

void command_owner::help()
{
	cout << " -owner <name>" << endl
	     << "\tOwner recorded in files and directories created afterwards" << endl
	     << "\t(DOS/50's USER owner; default NO.NAME)." << endl;
}

void command_owner::process(list<string> &pars)
{
	if (pars.size() != 1 || pars.front().size() > 14) { error("usage: -owner <name> (14 characters max)"); return; }
	string err;
	if (fs && !fs->set_owner(pars.front(), err)) { error(err); return; }
	owner = pars.front();
}
