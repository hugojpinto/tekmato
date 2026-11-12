// Image-level commands: -open -new -save -saveas -info -rename -owner
#include "command.hpp"
#include <cctype>
#include <cstdio>

using namespace std;

void command_open::help()
{
	cout << " -open <file.[img|imd]>" << endl
	     << "\tOpen a DOS/50 volume (a raw 8501 image of any of its four formats, or" << endl
	     << "\tan ImageDisk dump) or a TEKDOS volume (a raw 8002 image of 315,392" << endl
	     << "\tbytes). The size tells which." << endl;
}

void command_open::process(list<string> &pars)
{
	if (pars.size() != 1) { error("usage: -open <file>"); return; }
	string err;
	FILE *f = fopen(pars.front().c_str(), "rb");
	long size = -1;
	if (f) { fseek(f, 0, SEEK_END); size = ftell(f); fclose(f); }
	if (size == tekdos::IMAGE) {
		if (!tvol.load(pars.front(), err)) { error(err); return; }
		tk = true; dirty = false;
		info("TEKDOS volume " + tvol.ident() + " (" + to_string(tvol.entries().size()) + " files, " + to_string(tvol.free_blocks()) + " free blocks)");
		return;
	}
	if (!vol.load(pars.front(), err)) { error(err); return; }
	tk = false; dirty = false;
	info("volume " + vol.volume_name() + " (" + to_string(vol.maxfiles()) + " files max, " + to_string(vol.nblocks()) + " blocks)");
}

void command_new::help()
{
	cout << " -new [<maxfiles>] [<volume>] [<format>]" << endl
	     << "\tCreate an empty volume in memory. Formats:" << endl
	     << "\t  dos50-dsdd (default), dos50-dssd, dos50-ssdd, dos50-sssd: a DOS/50" << endl
	     << "\t    data volume for the 8550's 8501, laid out exactly as DOS/50's" << endl
	     << "\t    FORMAT does (default 256 files, volume TEKMATO); single or double" << endl
	     << "\t    sided, single (FORMAT:S) or double density. Not bootable. The root" << endl
	     << "\t    belongs to the -owner user (default NO.NAME, DOS/50's default user)." << endl
	     << "\t  tkdos-sssd: a TEKDOS disc for the 8002's Flexible Disc Unit (77" << endl
	     << "\t    tracks, 32 hard sectors of 128 bytes), as TEKDOS's FORMAT leaves it:" << endl
	     << "\t    tracks 1-4 reserved for TEKDOS. <volume> is the disc name (48" << endl
	     << "\t    characters); <maxfiles> does not apply (78 files)." << endl
	     << "\tFollow with -saveas." << endl;
}

void command_new::process(list<string> &pars)
{
	int maxfiles = 256;
	string name = "TEKMATO";
	dos50::geometry g;
	bool have_max = false, have_name = false, tekdos = false;
	for (auto &p : pars) {
		if (p == "tkdos-sssd") { tekdos = true; continue; }
		if (dos50::geometry::from_name(p, g)) continue;
		if (!have_max && !p.empty() && isdigit(static_cast<unsigned char>(p[0]))) { maxfiles = atoi(p.c_str()); have_max = true; }
		else if (!have_name) { name = p; have_name = true; }
		else { error("usage: -new [<maxfiles>] [<volume>] [dos50-sssd|dos50-ssdd|dos50-dssd|dos50-dsdd|tkdos-sssd]"); return; }
	}
	if (tekdos) {
		if (have_max) { error("TEKDOS discs always hold 78 files: no <maxfiles>"); return; }
		if (name.size() > size_t(tekdos::IDENT_LEN)) { error("TEKDOS disc names have at most 48 characters"); return; }
		tvol.create(name);
		tk = true; dirty = true;
		info("new tkdos-sssd volume " + name + ", " + to_string(tvol.free_blocks()) + " free blocks");
		return;
	}
	if (name.size() > 14) { error("volume names have at most 14 characters"); return; }
	string err;
	if (!vol.create(maxfiles, name, owner, g, err)) { error(err); return; }
	tk = false; dirty = true;
	info("new " + g.format_name() + " volume " + name + ", " + to_string(maxfiles) + " files, " + to_string(vol.free_blocks()) + " free blocks");
}

void command_save::help()
{
	cout << " -save" << endl << "\tWrite the volume back to the file it was opened from." << endl;
}

void command_save::process(list<string> &)
{
	if (!need_volume()) return;
	const string path = tk ? tvol.path() : vol.path();
	if (path.empty()) { error("no file name yet: use -saveas"); return; }
	string err;
	if (!(tk ? tvol.save(path, err) : vol.save(path, err))) { error(err); return; }
	dirty = false;
	info("saved " + path);
}

void command_saveas::help()
{
	cout << " -saveas <file.[img|imd]>" << endl
	     << "\tWrite the volume to a new file: .imd writes ImageDisk in the 8501's" << endl
	     << "\tphysical format, anything else a raw linear image (TEKDOS: raw only)." << endl;
}

void command_saveas::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (pars.size() != 1) { error("usage: -saveas <file>"); return; }
	string err;
	const string &f = pars.front();
	if (tk && f.size() > 4 && (f.compare(f.size() - 4, 4, ".imd") == 0 || f.compare(f.size() - 4, 4, ".IMD") == 0)) {
		error("TEKDOS discs are hard-sectored: ImageDisk cannot hold them; save a raw .img"); return;
	}
	if (!(tk ? tvol.save(f, err) : vol.save(f, err))) { error(err); return; }
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
	if (tk) {
		cout << "  Format         tkdos-sssd: 77 tracks, 32 hard sectors of 128 bytes, single sided" << endl
		     << "  Disc name      " << tvol.ident() << endl
		     << "  Files          " << tvol.entries().size() << " of 78 (sector 0 counts " << tvol.active_count() << ")" << endl
		     << "  Blocks         " << tekdos::BLOCKS << " x 1024 bytes (tracks 1-76)" << endl
		     << "  Blocks used    " << tvol.used_blocks() << endl
		     << "  Free blocks    " << tvol.free_blocks() << endl
		     << "  Bad blocks     " << tvol.bad_blocks() << endl;
		return;
	}
	const dos50::geometry &g = vol.geom();
	cout << "  Signature      " << vol.signature() << endl
	     << "  Format         " << g.format_name() << ": " << (g.ds ? 2 : 1) << " side" << (g.ds ? "s" : "") << ", " << (g.dd ? "double" : "single")
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
	cout << " -rename <volume>" << endl << "\tChange the volume name (DOS/50: 14 characters max; TEKDOS disc name: 48)." << endl;
}

void command_rename::process(list<string> &pars)
{
	if (!need_volume()) return;
	if (tk) {
		if (pars.size() != 1 || pars.front().size() > size_t(tekdos::IDENT_LEN)) { error("usage: -rename <name> (48 characters max)"); return; }
		tvol.set_ident(pars.front()); dirty = true; return;
	}
	if (pars.size() != 1 || pars.front().size() > 14 || pars.front().empty()) { error("usage: -rename <volume> (1-14 characters)"); return; }
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
