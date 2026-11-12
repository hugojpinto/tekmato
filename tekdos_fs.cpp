// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// TEKDOS behind tekmato's filesystem interface (see filesystem.hpp, FORMAT-TEKDOS.md).
// One flat directory: the root, whose id is ROOT; files are identified by their slot.
#include "filesystem.hpp"
#include "tekdos_volume.hpp"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iomanip>

namespace tekmato {
namespace {

using namespace tekdos;

constexpr int ROOT = 1000;                       // above every slot number
const char *const SOF = "\x0f";                  // the system overlay file, shown as '.'
const char *const SOF_HOST = "SYSTEM.SOF";       // its host name ('.' is not a TEKDOS character, so no clash)

bool ends_with_imd(const std::string &p)
{
	if (p.size() < 4) return false;
	std::string e = p.substr(p.size() - 4);
	for (auto &c : e) c = char(tolower(static_cast<unsigned char>(c)));
	return e == ".imd";
}

class tekdos_fs : public filesystem {
public:
	volume vol;

	const char *family() const override { return "TEKDOS"; }
	std::string format_name() const override { return "tkdos-sssd"; }
	std::string summary() const override {
		return "TEKDOS volume " + vol.ident() + " (" + std::to_string(vol.entries().size()) + " files, " + std::to_string(vol.free_blocks()) + " free blocks)";
	}
	const std::string &path() const override { return vol.path(); }
	bool save(const std::string &path, std::string &err) override {
		if (ends_with_imd(path)) { err = "TEKDOS discs are hard-sectored: ImageDisk cannot hold them; save a raw .img"; return false; }
		return vol.save(path, err);
	}
	std::string volume_name() const override { return vol.ident(); }
	bool set_volume_name(const std::string &n, std::string &err) override {
		if (n.size() > size_t(IDENT_LEN)) { err = "TEKDOS disc names have at most 48 characters"; return false; }
		vol.set_ident(n); return true;
	}

	bool valid_name(const std::string &n, std::string &why) const override { return n == SOF || n == "." || volume::valid_name(n, why); }
	// upper case, the legal characters, 8 at most, a letter first; '.' becomes ';'
	// (the suffix convention: HELLO.ASM -> HELLO;AS)
	std::string name_from_host(const std::string &h) const override {
		std::string base = h.substr(h.find_last_of('/') + 1), s;
		if (base == SOF_HOST) return SOF;
		for (unsigned char c : base) {
			if (s.size() == size_t(NAME_LEN)) break;
			c = static_cast<unsigned char>(toupper(c));
			if (s.empty() && !(c >= 'A' && c <= 'Z')) continue;
			if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("!\"#%&'()*;=?@", c)) s += char(c);
			else if (c == '.') s += ';';
		}
		return s;
	}
	std::string host_name(const std::string &n) const override { return n == SOF || n == "." ? SOF_HOST : n; }

	int root() const override { return ROOT; }
	std::vector<node> list_dir(int dir) override {
		std::vector<node> v;
		if (dir == ROOT) for (auto &e : vol.entries()) v.push_back({ e.slot, e.name, false });
		return v;
	}
	bool is_dir(int id) override { return id == ROOT; }
	int resolve(const std::string &p, std::string &err) override {
		std::string leaf;
		if (!flat(p, leaf, err)) return -1;
		if (leaf.empty()) return ROOT;
		int s = vol.find(leaf);
		if (s < 0) err = "file not found: " + leaf;
		return s;
	}
	int resolve_parent(const std::string &p, std::string &leaf, std::string &err) override { return flat(p, leaf, err) ? ROOT : -1; }
	int find(int dir, const std::string &name) override { return dir == ROOT ? vol.find(name) : -1; }

	bytes read_file(int id) override { return vol.read_file(id); }
	int write_file(int dir, const std::string &name, const bytes &data, std::string &err) override {
		if (dir != ROOT) return refuse("directories", err), -1;
		return name == SOF || name == "." ? vol.write_system_overlay(data, err) : vol.write_file(name, data, err);
	}
	bool remove(int dir, const std::string &name, std::string &err) override { return dir == ROOT ? vol.remove(name, err) : refuse("directories", err); }
	bool rename(int dir, const std::string &name, int new_dir, const std::string &new_name, std::string &err) override {
		if (dir != ROOT || new_dir != ROOT) return refuse("directories", err);
		return vol.rename(name, new_name, err);
	}

	void print_info(std::ostream &out) override {
		out << "  Format         tkdos-sssd: 77 tracks, 32 hard sectors of 128 bytes, single sided" << std::endl
		    << "  Disc name      " << vol.ident() << std::endl
		    << "  Files          " << vol.entries().size() << " of 78 (sector 0 counts " << vol.active_count() << ")" << std::endl
		    << "  Blocks         " << BLOCKS << " x 1024 bytes (tracks 1-76)" << std::endl
		    << "  Blocks used    " << vol.used_blocks() << std::endl
		    << "  Free blocks    " << vol.free_blocks() << std::endl
		    << "  Bad blocks     " << vol.bad_blocks() << std::endl;
	}
	// as TEKDOS's LDIR / prints it (the photographed printout of Tek8002_1a)
	void print_list(std::ostream &out, int, bool, bool) override {
		using std::left; using std::right; using std::setw;
		out << std::endl << "  " << left << setw(10) << "FILE NAME" << right << setw(5) << "BLKS" << setw(5) << "SEC" << setw(5) << "BYT"
		    << "   " << left << setw(30) << "IDENT" << setw(11) << "LOAD" << "START" << right << std::endl;
		for (auto &e : vol.entries()) {
			out << "  " << left << setw(10) << volume::display_name(e.name) << right << setw(5) << e.blocks << setw(5) << e.sectors << setw(5) << e.last;
			auto h = vol.header(e.slot);
			if (h.valid) {
				char ld[16], st[8];
				snprintf(ld, sizeof ld, "%X-%X", h.load_lo, h.load_hi); snprintf(st, sizeof st, "%X", h.start);
				out << "   " << left << setw(30) << h.ident << setw(11) << ld << st << right;
			}
			out << std::endl;
		}
		out << std::endl << "  TOTAL FILES       " << setw(4) << vol.active_count() << std::endl
		    << "  TOTAL BLOCKS USED " << setw(4) << vol.used_blocks() << std::endl
		    << "  BLOCKS AVAILABLE  " << setw(4) << vol.free_blocks() << std::endl
		    << "  TOTAL BAD BLOCKS  " << setw(4) << vol.bad_blocks() << std::endl;
	}
	bool check(std::ostream &out, bool) override {
		auto r = vol.check();
		for (auto &p : r.problems) out << "W : " << p << std::endl;
		return r.problems.empty();
	}

private:
	// a path names at most one file of the root
	bool flat(const std::string &p, std::string &leaf, std::string &err) {
		leaf = p;
		while (!leaf.empty() && leaf.front() == '/') leaf.erase(0, 1);
		if (leaf.find('/') != std::string::npos) return refuse("directories", err);
		return true;
	}
};

} // namespace

bool probe_tekdos(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	const long size = ftell(f);
	fclose(f);
	return size == IMAGE;
}

std::unique_ptr<filesystem> open_tekdos(const std::string &path, std::string &err)
{
	auto fs = std::make_unique<tekdos_fs>();
	if (!fs->vol.load(path, err)) return nullptr;
	return fs;
}

std::unique_ptr<filesystem> create_tekdos(const char *, const new_volume &v, std::string &err)
{
	if (v.maxfiles >= 0) { err = "TEKDOS discs always hold 78 files: no <maxfiles>"; return nullptr; }
	if (v.name.size() > size_t(IDENT_LEN)) { err = "TEKDOS disc names have at most 48 characters"; return nullptr; }
	auto fs = std::make_unique<tekdos_fs>();
	fs->vol.create(v.name);
	return fs;
}

} // namespace tekmato
