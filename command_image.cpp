// Image-level commands: -open -new -save -saveas -info -rename -owner
#include "command.hpp"
#include <cctype>

using namespace std;

void command_open::help()
{
	cout << " -open <file.[img|imd]>" << endl
	     << "\tOpen a DOS/50 volume: a raw 8501 image (3991 x 256-byte records)" << endl
	     << "\tor an ImageDisk (.imd) dump of an 8501 floppy." << endl;
}

void command_open::process(list<string> &pars)
{
	if (pars.size() != 1) { error("usage: -open <file>"); return; }
	string err;
	if (!vol.load(pars.front(), err)) { error(err); return; }
	dirty = false;
	info("volume " + vol.volume_name() + " (" + to_string(vol.maxfiles()) + " files max, " + to_string(vol.nblocks()) + " blocks)");
}

void command_new::help()
{
	cout << " -new [<maxfiles>] [<volume>] [sssd|ssdd|dssd|dsdd]" << endl
	     << "\tCreate an empty DOS/50 data volume in memory, laid out exactly as" << endl
	     << "\tDOS/50's FORMAT does (default 256 files, volume TEKMATO, dsdd =" << endl
	     << "\tthe 8501's double-sided double-density disc). Single or double" << endl
	     << "\tsided, single (FORMAT:S) or double density. Not bootable. The root" << endl
	     << "\tbelongs to the -owner user (default NO.NAME, DOS/50's default" << endl
	     << "\tuser), who can then write to it. Follow with -saveas." << endl;
}

void command_new::process(list<string> &pars)
{
	int maxfiles = 256;
	string name = "TEKMATO";
	dos50::geometry g;
	bool have_max = false, have_name = false;
	for (auto &p : pars) {
		if (dos50::geometry::from_name(p, g)) continue;
		if (!have_max && !p.empty() && isdigit(static_cast<unsigned char>(p[0]))) { maxfiles = atoi(p.c_str()); have_max = true; }
		else if (!have_name) { name = p; have_name = true; }
		else { error("usage: -new [<maxfiles>] [<volume>] [sssd|ssdd|dssd|dsdd]"); return; }
	}
	if (name.size() > 8) { error("volume names have at most 8 characters"); return; }
	string err;
	if (!vol.create(maxfiles, name, owner, g, err)) { error(err); return; }
	dirty = true;
	info("new " + g.name() + " volume " + name + ", " + to_string(maxfiles) + " files, " + to_string(vol.free_blocks()) + " free blocks");
}

void command_save::help()
{
	cout << " -save" << endl << "\tWrite the volume back to the file it was opened from." << endl;
}

void command_save::process(list<string> &)
{
	if (!need_volume()) return;
	if (vol.path().empty()) { error("no file name yet: use -saveas"); return; }
	string err;
	if (!vol.save(vol.path(), err)) { error(err); return; }
	dirty = false;
	info("saved " + vol.path());
}

void command_saveas::help()
{
	cout << " -saveas <file.[img|imd]>" << endl
	     << "\tWrite the volume to a new file: .imd writes ImageDisk in the 8501's" << endl
	     << "\tphysical format, anything else a raw linear image." << endl;
}

void command_saveas::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.size() != 1) { error("usage: -saveas <file>"); return; }
	string err;
	if (!vol.save(pars.front(), err)) { error(err); return; }
	dirty = false;
	info("saved " + pars.front());
}

void command_info::help()
{
	cout << " -info" << endl << "\tShow the volume label, layout and free space." << endl;
}

void command_info::process(list<string> &)
{
	if (!need_volume()) return;
	const dos50::geometry &g = vol.geom();
	cout << "  Signature      " << vol.signature() << endl
	     << "  Disc           " << g.name() << ": " << (g.ds ? 2 : 1) << " side" << (g.ds ? "s" : "") << ", " << (g.dd ? "double" : "single")
	     << " density, " << g.records() << " records" << endl
	     << "  Volume         " << vol.volume_name() << endl
	     << "  Formatted      " << dos50::volume::format_time(vol.label_time()) << endl
	     << "  Blocks         " << vol.nblocks() << " x 512 bytes" << endl
	     << "  Descriptors    blocks 6-" << 6 + vol.maxfiles() - 1 << " (" << vol.maxfiles() << " files max)" << endl
	     << "  Block bitmap   block " << vol.bitmap_block() << endl
	     << "  Bad-block map  block " << vol.badmap_block() << endl
	     << "  File bitmap    block " << vol.descmap_block() << endl
	     << "  Root directory block " << vol.root_block() << endl
	     << "  Files used     " << vol.maxfiles() - vol.free_files() << endl
	     << "  Free files     " << vol.free_files() << endl
	     << "  Free blocks    " << vol.free_blocks() << endl
	     << "  Bad blocks     " << vol.bad_blocks() << endl;
}

void command_rename::help()
{
	cout << " -rename <volume>" << endl << "\tChange the volume name (8 characters max)." << endl;
}

void command_rename::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.size() != 1 || pars.front().size() > 8 || pars.front().empty()) { error("usage: -rename <volume> (1-8 characters)"); return; }
	vol.set_volume_name(pars.front());
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
	owner = pars.front();
}
