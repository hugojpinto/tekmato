// TEKDOS volume: see tekdos_volume.hpp and FORMAT-TEKDOS.md.
#include "tekdos_volume.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>

namespace tekdos {

bool volume::load(const std::string &path, std::string &err)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) { err = "cannot open " + path; return false; }
	std::vector<u8> img((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (img.size() != size_t(IMAGE)) { err = path + ": a TEKDOS image is 315,392 bytes (77 x 32 x 128)"; return false; }
	m_img.swap(img); m_path = path;
	return true;
}

bool volume::save(const std::string &path, std::string &err) const
{
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	if (!f) { err = "cannot write " + path; return false; }
	f.write(reinterpret_cast<const char *>(m_img.data()), std::streamsize(m_img.size()));
	if (!f) { err = "write failed: " + path; return false; }
	return true;
}

// TEKDOS's FORMAT, as the freshly formatted disc Tek8002_1b ("FORMATTED") shows: data
// sectors zero, one directory entry TEKDOS holding blocks 0-15 (tracks 1-4), 128 sectors
// with 128 bytes in the last, and those blocks marked in the master map.
void volume::create(const std::string &ident)
{
	m_img.assign(IMAGE, 0); m_path.clear();
	set_ident(ident);
	memcpy(name_at(0), "TEKDOS  ", NAME_LEN);
	u8 *r = record(0);
	for (int b = 0; b < SYSTEM_BLOCKS; b++) { set_bit(r, b, true); set_bit(master(), b, true); }
	r[38] = 0; r[39] = SYSTEM_BLOCKS * BLOCK_SECTORS; r[40] = SECTOR;
	update_count();
}

std::string volume::ident() const
{
	std::string s;
	for (int i = 3; i < 52 && m_img[i] != '\r'; i++) s += char(m_img[i]);
	while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
	return s;
}

void volume::set_ident(const std::string &s)
{
	std::string t = s.substr(0, IDENT_LEN);
	memset(&m_img[3], 0, 49);
	memcpy(&m_img[3], t.data(), t.size());
	m_img[3 + t.size()] = '\r';
}

bool volume::slot_used(int slot) const
{
	const u8 *n = name_at(slot);
	return std::any_of(n, n + NAME_LEN, [](u8 c) { return c != 0; });
}

std::vector<int> volume::file_blocks(int slot) const
{
	std::vector<int> b;
	const u8 *r = record(slot);
	for (int i = 0; i < BLOCKS; i++) if (bit(r, i)) b.push_back(i);
	return b;
}

std::vector<entry> volume::entries() const
{
	std::vector<entry> v;
	for (int s = 0; s < SLOTS; s++) {
		if (!slot_used(s)) continue;
		std::string n(reinterpret_cast<const char *>(name_at(s)), NAME_LEN);
		while (!n.empty() && (n.back() == ' ' || n.back() == '\0')) n.pop_back();
		const u8 *r = record(s);
		v.push_back({ s, n, int(file_blocks(s).size()), (r[38] << 8) | r[39], r[40] });
	}
	return v;
}

int volume::find(const std::string &name) const
{
	std::string want = name == "." ? "\x0f" : name;
	for (auto &e : entries()) if (e.name == want) return e.slot;
	return -1;
}

std::vector<u8> volume::read_file(int slot) const
{
	const u8 *r = record(slot);
	int sectors = (r[38] << 8) | r[39], last = r[40];
	size_t size = sectors ? size_t(sectors - 1) * SECTOR + std::min(last, SECTOR) : 0;
	std::vector<u8> d;
	for (int b : file_blocks(slot)) {                // blocks in ascending order, sectors in order within a block
		if (d.size() >= size) break;
		d.insert(d.end(), &m_img[block_offset(b)], &m_img[block_offset(b)] + BLOCK);
	}
	d.resize(std::min(size, d.size()));
	return d;
}

module_header volume::header(int slot) const
{
	module_header h;
	auto b = file_blocks(slot);
	if (b.empty()) return h;
	const u8 *p = &m_img[block_offset(b[0])];
	if (memcmp(p, "MOD", 3) != 0) return h;
	h.valid = true;
	h.load_lo = u16(p[4] << 8 | p[5]); h.load_hi = u16(p[6] << 8 | p[7]); h.start = u16(p[8] << 8 | p[9]);
	for (int i = 10; i < SECTOR && p[i] != '\r'; i++) if (p[i] >= 0x20 && p[i] < 0x7f) h.ident += char(p[i]);
	return h;
}

// TEKDOS file names (users manual 070-2701 §2, reference booklet 070-2706): 1-8 characters
// from A-Z, 0-9 and ! " # % & ' ( ) * ; = ? @, starting with a letter; not a device name.
bool volume::valid_name(const std::string &n, std::string &why)
{
	static const char *const devices[] = { "CONI", "CONO", "LPT1", "PPTR", "PPTP", "REMI", "REMO", "TTYR", "TTYP", nullptr };
	if (n.empty() || n.size() > size_t(NAME_LEN)) { why = "TEKDOS names have 1 to 8 characters"; return false; }
	if (!(n[0] >= 'A' && n[0] <= 'Z')) { why = "TEKDOS names start with a letter (A-Z)"; return false; }
	for (char c : n)
		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("!\"#%&'()*;=?@", c))) {
			why = std::string("character '") + c + "' is not allowed in TEKDOS names"; return false;
		}
	for (int i = 0; devices[i]; i++) if (n == devices[i]) { why = n + " is a device name"; return false; }
	return true;
}

void volume::update_count()
{
	int n = 0;
	for (int s = 0; s < SLOTS; s++) if (slot_used(s)) n++;
	m_img[2] = u8(n);
}

void volume::free_slot(int slot)
{
	for (int b : file_blocks(slot)) set_bit(master(), b, false);
	memset(name_at(slot), 0, NAME_LEN);       // TEKDOS's DELETE zeroes the name (observed)
	memset(record(slot), 0, 41);
}

int volume::write_file(const std::string &name, const std::vector<u8> &data, std::string &err)
{
	if (!valid_name(name, err)) return -1;
	return store(name, data, err);
}

int volume::write_system_overlay(const std::vector<u8> &data, std::string &err) { return store("\x0f", data, err); }

int volume::store(const std::string &name, const std::vector<u8> &data, std::string &err)
{
	const int sectors = int((data.size() + SECTOR - 1) / SECTOR);
	const int need = (sectors + BLOCK_SECTORS - 1) / BLOCK_SECTORS;
	if (sectors > 0xffff) { err = "file too large"; return -1; }
	int slot = find(name);
	const int had = slot >= 0 ? int(file_blocks(slot).size()) : 0;
	if (need > free_blocks() + had) { err = "disc full: " + std::to_string(need) + " blocks needed, " + std::to_string(free_blocks() + had) + " free"; return -1; }
	if (slot < 0) {
		for (int s = 0; s < SLOTS && slot < 0; s++) if (!slot_used(s)) slot = s;
		if (slot < 0) { err = "directory full (78 files)"; return -1; }
	} else free_slot(slot);
	// lowest free blocks first (tekmato's choice: TEKDOS reads a file's blocks in ascending order)
	std::vector<int> blocks;
	for (int b = 0; b < BLOCKS && int(blocks.size()) < need; b++) if (!bit(master(), b)) blocks.push_back(b);
	u8 *r = record(slot);
	memset(r, 0, 41);
	for (size_t i = 0; i < blocks.size(); i++) {
		set_bit(master(), blocks[i], true); set_bit(r, blocks[i], true);
		u8 *p = &m_img[block_offset(blocks[i])];
		memset(p, 0, BLOCK);
		const size_t from = i * BLOCK, n = std::min(size_t(BLOCK), data.size() - from);
		memcpy(p, data.data() + from, n);
	}
	r[38] = u8(sectors >> 8); r[39] = u8(sectors);
	r[40] = u8(sectors ? data.size() - size_t(sectors - 1) * SECTOR : 0);
	std::string padded = name; padded.resize(NAME_LEN, ' ');
	memcpy(name_at(slot), padded.data(), NAME_LEN);
	update_count();
	return slot;
}

bool volume::remove(const std::string &name, std::string &err)
{
	int s = find(name);
	if (s < 0) { err = "file not found: " + name; return false; }
	free_slot(s);
	update_count();
	return true;
}

bool volume::rename(const std::string &from, const std::string &to, std::string &err)
{
	int s = find(from);
	if (s < 0) { err = "file not found: " + from; return false; }
	if (!valid_name(to, err)) return false;
	if (find(to) >= 0) { err = "file exists: " + to; return false; }
	std::string padded = to; padded.resize(NAME_LEN, ' ');
	memcpy(name_at(s), padded.data(), NAME_LEN);
	return true;
}

int volume::free_blocks() const
{
	int n = 0;
	for (int b = 0; b < BLOCKS; b++) if (!bit(master(), b)) n++;
	return n;
}

int volume::bad_blocks() const
{
	int n = 0;
	for (int b = 0; b < BLOCKS; b++) if (bit(badmap(), b)) n++;
	return n;
}

int volume::used_blocks() const { return BLOCKS - free_blocks(); }

// The master map must be the union of the files' maps and the bad-block map; files must
// not share blocks; each file needs ceil(sectors/8) blocks; byte 2 counts the named slots.
check_report volume::check() const
{
	check_report r;
	std::set<int> owned;
	auto es = entries();
	for (auto &e : es) {
		auto b = file_blocks(e.slot);
		for (int x : b) {
			if (owned.count(x)) r.problems.push_back("block " + std::to_string(x) + " is in more than one file (" + display_name(e.name) + ")");
			if (bit(badmap(), x)) r.problems.push_back(display_name(e.name) + " uses bad block " + std::to_string(x));
			owned.insert(x);
		}
		if (int(b.size()) != (e.sectors + BLOCK_SECTORS - 1) / BLOCK_SECTORS)
			r.problems.push_back(display_name(e.name) + ": " + std::to_string(b.size()) + " blocks for " + std::to_string(e.sectors) + " sectors");
		if (e.sectors ? (e.last < 1 || e.last > SECTOR) : e.last != 0)
			r.problems.push_back(display_name(e.name) + ": " + std::to_string(e.last) + " bytes in the last sector");
	}
	for (int x = 0; x < BLOCKS; x++) {
		const bool want = owned.count(x) || bit(badmap(), x);
		if (want && !bit(master(), x)) r.problems.push_back("block " + std::to_string(x) + " is in use but free in the master map");
		if (!want && bit(master(), x)) r.problems.push_back("block " + std::to_string(x) + " is marked in use but belongs to no file");
	}
	for (int s = 0; s < SLOTS; s++)
		if (!slot_used(s) && std::any_of(record(s), record(s) + 41, [](u8 c) { return c != 0; }))
			r.problems.push_back("unnamed slot " + std::to_string(s) + " has a block map");
	if (active_count() != int(es.size()))
		r.problems.push_back("sector 0 counts " + std::to_string(active_count()) + " files, the directory names " + std::to_string(es.size()));
	return r;
}

} // namespace tekdos
