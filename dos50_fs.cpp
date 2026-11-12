// DOS/50 behind tekmato's filesystem interface (see filesystem.hpp, FORMAT.md).
#include "filesystem.hpp"
#include "dos50_volume.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <set>

namespace tekmato {
namespace {

using namespace dos50;

class dos50_fs : public filesystem {
public:
	explicit dos50_fs(const std::string &owner) : m_owner(owner) {}
	volume vol;

	const char *family() const override { return "DOS/50"; }
	std::string format_name() const override { return vol.geom().format_name(); }
	std::string summary() const override {
		return "volume " + vol.volume_name() + " (" + std::to_string(vol.maxfiles()) + " files max, " + std::to_string(vol.nblocks()) + " blocks)";
	}
	const std::string &path() const override { return vol.path(); }
	bool save(const std::string &path, std::string &err) override { return vol.save(path, err); }
	std::string volume_name() const override { return vol.volume_name(); }
	bool set_volume_name(const std::string &n, std::string &err) override {
		if (n.empty() || n.size() > 14) { err = "DOS/50 volume names have 1 to 14 characters"; return false; }
		vol.set_volume_name(n); return true;
	}

	bool valid_name(const std::string &n, std::string &why) const override {
		if (volume::valid_name(n)) return true;
		why = "invalid DOS/50 name: " + n; return false;
	}
	// legal characters, 14 at most, case kept; spaces become '_'
	std::string name_from_host(const std::string &h) const override {
		std::string base = h.substr(h.find_last_of('/') + 1), s;
		for (unsigned char c : base) {
			if (s.size() == size_t(NAME_LEN)) break;
			if (c > ' ' && c < 0x7f && c != '/' && c != '*' && c != '?') s += char(c);
			else if (c == ' ') s += '_';
		}
		if (!volume::valid_name(s)) {        // brackets that are not one abbreviation pair: drop them
			std::string t;
			for (char c : s) if (c != '[' && c != ']') t += c;
			s = t;
		}
		return s;
	}

	int root() const override { return 0; }
	std::vector<node> list_dir(int dir) override {
		std::vector<node> v;
		for (auto &e : vol.list_dir(dir)) v.push_back({ e.id, e.name, vol.desc(e.id).is_dir() });
		return v;
	}
	bool is_dir(int id) override { return vol.desc(id).is_dir(); }
	int resolve(const std::string &p, std::string &err) override { return vol.resolve(p, err); }
	int resolve_parent(const std::string &p, std::string &leaf, std::string &err) override { return vol.resolve_parent(p, leaf, err); }
	int find(int dir, const std::string &name) override { return vol.find(dir, name); }

	bytes read_file(int id) override { return vol.read_file(id); }
	int write_file(int dir, const std::string &name, const bytes &data, std::string &err) override {
		const u32 now = volume::now_local();
		for (auto &e : vol.list_dir(dir))
			if (e.name == name) return vol.replace_data(e.id, data, now, err) ? e.id : -1;
		return vol.create_file(dir, name, data, now, m_owner, err);
	}
	bool remove(int dir, const std::string &name, std::string &err) override { return delete_tree(dir, name, err, 0); }
	bool rename(int dir, const std::string &name, int new_dir, const std::string &new_name, std::string &err) override;
	int make_dir(int dir, const std::string &name, std::string &err) override { return vol.make_dir(dir, name, volume::now_local(), m_owner, err); }
	bool link(int id, int dir, const std::string &name, std::string &err) override {
		if (vol.desc(id).is_dir()) { err = "directories cannot be linked"; return false; }
		return vol.link(id, dir, name, err);
	}
	bool attributes(int id, const std::string &path, const std::list<std::string> &changes, std::ostream &out, std::string &err) override;
	bool set_owner(const std::string &o, std::string &err) override {
		if (o.size() > 14) { err = "owner names have at most 14 characters"; return false; }
		m_owner = o; return true;
	}

	void print_info(std::ostream &out) override;
	void print_list(std::ostream &out, int dir, bool recurse, bool ids) override;
	bool check(std::ostream &out, bool fix) override;

private:
	std::string m_owner;
	std::string stored_name(int dir, int id, const std::string &name) {   // find() also accepts abbreviations
		for (auto &e : vol.list_dir(dir)) if (e.id == id && volume::name_matches(e.name, name)) return e.name;
		return name;
	}
	bool delete_tree(int dir, const std::string &name, std::string &err, int depth);
	void list_dir_rec(std::ostream &out, int dir, const std::string &indent, bool recurse, bool ids, std::set<int> &open, int &files, int &dirs);
};

// DOS/50 DELETE: a file goes with its last link; a directory with everything in it
// (files still linked elsewhere survive)
bool dos50_fs::delete_tree(int dir, const std::string &name, std::string &err, int depth)
{
	int id = vol.find(dir, name);
	if (id < 0) { err = "file not found: " + name; return false; }
	const std::string stored = stored_name(dir, id, name);
	if (vol.desc(id).is_dir() && vol.desc(id).links() <= 1) {
		if (depth > 16) { err = "directory nesting too deep"; return false; }
		for (auto &e : vol.list_dir(id))
			if (!delete_tree(id, e.name, err, depth + 1)) return false;
	}
	return vol.unlink(dir, stored, err);
}

bool dos50_fs::rename(int dir, const std::string &name, int ndir, const std::string &nname, std::string &err)
{
	int id = vol.find(dir, name);
	if (id < 0) { err = "file not found: " + name; return false; }
	const std::string leaf = stored_name(dir, id, name);
	if (ndir == dir) return vol.rename_entry(dir, leaf, nname, err);
	if (vol.desc(id).is_dir()) {
		// refuse to move a directory into itself: walk up from the target
		for (int p = ndir, guard = 0; guard < 64; guard++) {
			if (p == id) { err = "cannot move a directory into itself"; return false; }
			bool up = false;
			for (int cand = 0; cand < vol.maxfiles() && !up; cand++)
				if (vol.id_in_use(cand) && vol.desc(cand).is_dir())
					for (auto &e : vol.list_dir(cand)) if (e.id == p && cand != p) { p = cand; up = true; break; }
			if (!up || p == 0) break;
		}
	}
	// link under the new name, then drop the old entry (while linked twice, unlink()
	// only drops the entry and the link count; the descriptor stays)
	return vol.link(id, ndir, nname, err) && vol.unlink(dir, leaf, err);
}

bool dos50_fs::attributes(int id, const std::string &path, const std::list<std::string> &changes, std::ostream &out, std::string &err)
{
	descriptor d = vol.desc(id);
	for (auto &p : changes) {
		if (p.compare(0, 4, "cap=") == 0 || p.compare(0, 4, "CAP=") == 0) {
			u8 caps = 0;
			std::string v = p.substr(4) + "/", tok;
			for (char c : v) {
				if (c != '/') { tok += char(toupper(static_cast<unsigned char>(c))); continue; }
				if (tok == "OR") caps |= CAP_OR;
				else if (tok == "OW") caps |= CAP_OW;
				else if (tok == "PR") caps |= CAP_PR;
				else if (tok == "PW") caps |= CAP_PW;
				else if (!tok.empty()) { err = "unknown capability " + tok + " (OR, OW, PR, PW)"; return false; }
				tok.clear();
			}
			d.set_caps(caps);
		} else if (p.compare(0, 6, "owner=") == 0 || p.compare(0, 6, "OWNER=") == 0) {
			if (p.size() - 6 > 14) { err = "owner names have at most 14 characters"; return false; }
			d.set_owner(p.substr(6));
		} else { err = "unknown attribute " + p; return false; }
	}
	out << "  " << path << std::endl
	    << "    CAPABILITIES=" << volume::caps_string(d.caps()) << std::endl
	    << "    CREATION=" << volume::format_time(d.ctime()) << std::endl
	    << "    UPDATE=" << volume::format_time(d.mtime()) << std::endl
	    << "    ACCESS=" << volume::format_time(d.atime()) << "  OWNER=" << d.owner() << std::endl
	    << "    SIZE=" << d.size() << "  CONSUMPTION=" << d.consumption() << "  LINK=" << int(d.links())
	    << "  TYPE=" << (d.is_dir() ? "DIR" : "FILE") << std::endl
	    << "    ID=" << id << "  EXTENTS=";
	for (auto &e : d.extents()) out << e.block << "+" << e.count << " ";
	out << std::endl;
	return true;
}

void dos50_fs::print_info(std::ostream &out)
{
	const geometry &g = vol.geom();
	out << "  Signature      " << vol.signature() << std::endl
	    << "  Format         " << g.format_name() << ": " << (g.ds ? 2 : 1) << " side" << (g.ds ? "s" : "") << ", " << (g.dd ? "double" : "single")
	    << " density, " << g.records() << " records" << std::endl
	    << "  Volume         " << vol.volume_name() << std::endl
	    << "  Formatted      " << volume::format_time(vol.label_time()) << std::endl
	    << "  Blocks         " << vol.nblocks() << " x 512 bytes" << std::endl
	    << "  Descriptors    blocks 6-" << 6 + vol.maxfiles() - 1 << " (" << vol.maxfiles() << " files max)" << std::endl
	    << "  Block bitmap   block " << vol.bitmap_block() << std::endl
	    << "  Bad-block map  block " << vol.badmap_block() << std::endl
	    << "  File bitmap    block " << vol.descmap_block() << std::endl
	    << "  Root directory block " << vol.root_block() << std::endl
	    << "  Files used     " << vol.maxfiles() - vol.free_files() << std::endl
	    << "  Free files     " << vol.free_files() << std::endl
	    << "  Free blocks    " << vol.free_blocks() << std::endl
	    << "  Bad blocks     " << vol.bad_blocks() << std::endl;
}

void dos50_fs::list_dir_rec(std::ostream &out, int dir, const std::string &indent, bool recurse, bool ids, std::set<int> &open, int &files, int &dirs)
{
	using std::left; using std::right; using std::setw;
	open.insert(dir);
	for (auto &e : vol.list_dir(dir)) {
		descriptor d = vol.desc(e.id);
		std::string name = e.name + (d.is_dir() ? "/" : "");
		out << "  " << indent << left << setw(std::max(1, 18 - int(indent.size()))) << name << right;
		if (d.is_dir()) out << setw(8) << "<DIR>";
		else out << setw(8) << d.size();
		out << setw(5) << d.consumption() << setw(3) << int(d.links()) << "  " << left << setw(12) << volume::caps_string(d.caps())
		    << setw(10) << d.owner() << right << volume::format_time(d.mtime());
		if (ids) out << "  #" << e.id;
		out << std::endl;
		if (d.is_dir()) dirs++; else files++;
		if (d.is_dir() && recurse && !open.count(e.id)) list_dir_rec(out, e.id, indent + "  ", recurse, ids, open, files, dirs);
	}
	open.erase(dir);
}

void dos50_fs::print_list(std::ostream &out, int dir, bool recurse, bool ids)
{
	using std::left; using std::right; using std::setw;
	out << "  " << left << setw(18) << "NAME" << right << setw(8) << "SIZE" << setw(5) << "BLK" << setw(3) << "LK"
	    << "  " << left << setw(12) << "CAP" << setw(10) << "OWNER" << right << "UPDATED" << std::endl;
	std::set<int> open;
	int files = 0, dirs = 0;
	list_dir_rec(out, dir, "", recurse, ids, open, files, dirs);
	out << "  " << files << " files, " << dirs << " directories; "
	    << vol.maxfiles() - vol.free_files() << " files used, " << vol.free_files() << " free files, "
	    << vol.free_blocks() << " free blocks" << std::endl;
}

bool dos50_fs::check(std::ostream &out, bool fix)
{
	auto r = vol.check();
	for (auto &p : r.problems) out << "W : " << p << std::endl;
	if (!r.orphans.empty()) {
		out << "I : " << r.orphans.size() << " orphan files holding " << r.orphan_blocks << " blocks:";
		for (int id : r.orphans) out << " #" << id;
		out << std::endl;
	}
	if (fix && !r.orphans.empty()) {
		int n = vol.free_orphans(r);
		out << "I : freed " << n << " orphans; " << vol.free_blocks() << " free blocks, " << vol.free_files() << " free files" << std::endl;
	}
	return r.problems.empty() && r.orphans.empty();
}

} // namespace

std::unique_ptr<filesystem> open_dos50(const std::string &path, const std::string &owner, std::string &err)
{
	auto fs = std::make_unique<dos50_fs>(owner);
	if (!fs->vol.load(path, err)) return nullptr;
	return fs;
}

std::unique_ptr<filesystem> create_dos50(const char *format_name, const new_volume &v, std::string &err)
{
	geometry g;
	if (!geometry::from_name(format_name, g)) { err = std::string("not a DOS/50 format: ") + format_name; return nullptr; }
	if (v.name.size() > 14) { err = "volume names have at most 14 characters"; return nullptr; }
	auto fs = std::make_unique<dos50_fs>(v.owner);
	if (!fs->vol.create(v.maxfiles < 0 ? 256 : v.maxfiles, v.name, v.owner, g, err)) return nullptr;
	return fs;
}

} // namespace tekmato
