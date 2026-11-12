// DOS/50 volume: image I/O, label, bitmaps, descriptors, directories.
#include "dos50_volume.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

namespace dos50 {

// ---------------------------------------------------------------- descriptor

std::vector<extent> descriptor::extents() const
{
	std::vector<extent> e;
	for (int i = 0; i < MAX_EXTENTS; i++) {
		u16 c = w16(i * 4 + 2);
		if (c == 0) break;               // a zero count ends the list
		e.push_back({ w16(i * 4), c });
	}
	return e;
}

void descriptor::set_extents(const std::vector<extent> &e)
{
	for (size_t i = 0; i < e.size(); i++) {
		u8 *q = m_p + i * 4;
		q[0] = u8(e[i].block); q[1] = u8(e[i].block >> 8); q[2] = u8(e[i].count); q[3] = u8(e[i].count >> 8);
	}
	if (e.size() < size_t(MAX_EXTENTS)) { u8 *q = m_p + e.size() * 4; q[2] = q[3] = 0; }
}

std::string descriptor::owner() const
{
	std::string s;
	for (int i = 0; i < 16 && m_p[0x1c8 + i]; i++) s += char(m_p[0x1c8 + i]);
	return s;
}

void descriptor::set_owner(const std::string &o)
{
	memset(m_p + 0x1c8, 0, 16);
	memcpy(m_p + 0x1c8, o.data(), std::min<size_t>(o.size(), 14));
}

static u16 rw16(const u8 *p) { return u16(p[0] | (p[1] << 8)); }
static void ww16(u8 *p, u16 v) { p[0] = u8(v); p[1] = u8(v >> 8); }

// ---------------------------------------------------------------- geometry

bool geometry::from_records(size_t n, geometry &g)
{
	for (int k = 0; k < 4; k++) {
		geometry c; c.ds = k & 2; c.dd = k & 1;
		if (size_t(c.records()) == n) { g = c; return true; }
	}
	return false;
}

bool geometry::from_name(const std::string &n, geometry &g)
{
	// dos50-sssd, dos50-ssdd, dos50-dssd, dos50-dsdd (the bare sssd ... dsdd of tekmato 0.2
	// are still accepted); lower case only, so upper-case volume names like "SSDD" stay names
	std::string u, s = n.compare(0, 6, "dos50-") == 0 ? n.substr(6) : n;
	for (char c : s) { if (isupper(static_cast<unsigned char>(c))) return false; u += char(toupper(static_cast<unsigned char>(c))); }
	if (u.size() != 4 || (u[0] != 'S' && u[0] != 'D') || u[1] != 'S' || (u[2] != 'S' && u[2] != 'D') || u[3] != 'D') return false;
	g.ds = u[0] == 'D'; g.dd = u[2] == 'D';
	return true;
}

// ---------------------------------------------------------------- image I/O

static bool decode_imd(const std::vector<u8> &d, std::vector<u8> &out, std::string &comment, std::string &err)
{
	size_t i = 0;
	while (i < d.size() && d[i] != 0x1a) i++;
	std::string hdr(d.begin(), d.begin() + i);
	size_t nl = hdr.find('\n');
	comment = nl == std::string::npos ? "" : hdr.substr(nl + 1);
	i++;
	std::map<int, std::vector<u8>> tracks;          // cyl*2+head, sectors in id order
	while (i + 5 <= d.size()) {
		u8 cyl = d[i + 1], head = d[i + 2], ns = d[i + 3], sz = d[i + 4]; i += 5;
		if (i + ns > d.size() || sz > 6) { err = "corrupt IMD track header"; return false; }
		std::vector<u8> smap(d.begin() + i, d.begin() + i + ns); i += ns;
		if (head & 0x80) i += ns;
		if (head & 0x40) i += ns;
		size_t bs = size_t(128) << sz;
		std::map<int, std::vector<u8>> secs;
		for (u8 s : smap) {
			if (i >= d.size()) { err = "truncated IMD"; return false; }
			u8 t = d[i++];
			std::vector<u8> data(bs, 0xe5);
			if (t == 0) {}
			else if (t % 2 == 0) { data.assign(bs, d[i]); i++; }
			else { if (i + bs > d.size()) { err = "truncated IMD"; return false; } std::copy(d.begin() + i, d.begin() + i + bs, data.begin()); i += bs; }
			secs[s] = data;
		}
		std::vector<u8> trk;
		for (auto &kv : secs) trk.insert(trk.end(), kv.second.begin(), kv.second.end());
		tracks[cyl * 2 + (head & 1)] = trk;
	}
	out.clear();
	for (auto &kv : tracks) out.insert(out.end(), kv.second.begin(), kv.second.end());
	return true;
}

// 8501 physical format for the geometry: 26 sectors per track, FM 128 or MFM 256 bytes.
static bool encode_imd(const std::vector<u8> &lin, const geometry &g, const std::string &comment, std::vector<u8> &out)
{
	char hdr[64];
	time_t now = time(nullptr); struct tm *t = localtime(&now);
	snprintf(hdr, sizeof hdr, "IMD 1.18: %02d/%02d/%04d %02d:%02d:%02d\r\n", t->tm_mday, t->tm_mon + 1, t->tm_year + 1900, t->tm_hour, t->tm_min, t->tm_sec);
	out.assign(hdr, hdr + strlen(hdr));
	out.insert(out.end(), comment.begin(), comment.end());
	out.push_back(0x1a);
	for (int trk = 0; trk < g.tracks(); trk++) {
		bool fm = g.fm(trk);
		size_t bs = fm ? 128 : 256, off = size_t(g.track_first(trk)) * RECORD;
		out.insert(out.end(), { u8(fm ? 0 : 3), u8(g.cylinder(trk)), u8(g.side(trk)), 26, u8(fm ? 0 : 1) });
		for (int s = 1; s <= 26; s++) out.push_back(u8(s));
		for (int s = 0; s < 26; s++, off += bs) {
			const u8 *p = off + bs <= lin.size() ? &lin[off] : nullptr;
			if (p && std::all_of(p, p + bs, [&](u8 b) { return b == p[0]; })) { out.push_back(2); out.push_back(p[0]); }
			else { out.push_back(1); for (size_t k = 0; k < bs; k++) out.push_back(p ? p[k] : 0xe5); }
		}
	}
	return true;
}

static bool ends_with_ci(const std::string &s, const std::string &suf)
{
	if (s.size() < suf.size()) return false;
	for (size_t i = 0; i < suf.size(); i++) if (tolower(s[s.size() - suf.size() + i]) != suf[i]) return false;
	return true;
}

bool volume::load(const std::string &path, std::string &err)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) { err = "cannot open " + path; return false; }
	std::vector<u8> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	std::vector<u8> lin;
	m_is_imd = d.size() >= 4 && d[0] == 'I' && d[1] == 'M' && d[2] == 'D';
	if (m_is_imd) { if (!decode_imd(d, lin, m_imd_comment, err)) return false; }
	else lin = d;
	// geometry from the record count; failing that (a truncated raw dump), from the
	// label's last block number, padding the image to the full size
	if (lin.size() % RECORD || !geometry::from_records(lin.size() / RECORD, m_geom)) {
		if (lin.size() < size_t(LABEL_BLOCK + 1) * BLOCK) { err = "image too small for a DOS/50 volume"; return false; }
		const u8 *l = &lin[size_t(LABEL_BLOCK) * BLOCK];
		int blocks = rw16(l + 0x32) + 1;
		bool found = false;
		for (int k = 3; k >= 0 && !found; k--) {
			geometry c; c.ds = k & 2; c.dd = k & 1;
			if (c.blocks() == blocks && lin.size() <= size_t(c.records()) * RECORD) { m_geom = c; found = true; }
		}
		if (!found) { err = "image size " + std::to_string(lin.size()) + " matches no 8501 disc geometry"; return false; }
		lin.resize(size_t(m_geom.records()) * RECORD, 0);
	}
	m_img = lin;
	m_path = path;
	if (!parse_label(err)) { m_img.clear(); return false; }
	return true;
}

bool volume::save(const std::string &path, std::string &err) const
{
	std::vector<u8> out;
	if (ends_with_ci(path, ".imd")) encode_imd(m_img, m_geom, m_imd_comment.empty() ? std::string("tekmato\r\n") : m_imd_comment, out);
	else out.assign(m_img.begin(), m_img.begin() + size_t(m_geom.records()) * RECORD);
	std::string tmp = path + ".tmp";
	{
		std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
		if (!f) { err = "cannot write " + tmp; return false; }
		f.write(reinterpret_cast<const char *>(out.data()), std::streamsize(out.size()));
		if (!f) { err = "write failed: " + tmp; return false; }
	}
	if (rename(tmp.c_str(), path.c_str()) != 0) { err = "cannot replace " + path; remove(tmp.c_str()); return false; }
	return true;
}

// ---------------------------------------------------------------- label


bool volume::parse_label(std::string &err)
{
	const u8 *l = blk(LABEL_BLOCK);
	if (memcmp(l, "TEKTRONIX DOS/50", 16) != 0) { err = "not a DOS/50 volume (no label in record 10)"; return false; }
	m_bmp = rw16(l + 0x2a); m_bad = rw16(l + 0x2c); m_dmap = rw16(l + 0x2e); m_root = rw16(l + 0x30);
	m_nblocks = rw16(l + 0x32) + 1; m_maxfiles = rw16(l + 0x34) + 1;   // 0x32 = last block number (FORMAT frees block 0x7CA too)
	int total = m_geom.blocks();
	if (m_nblocks <= 0 || m_nblocks > total || m_maxfiles <= 0 || FIRST_DESC_BLOCK + m_maxfiles > m_bmp ||
	    m_bmp >= m_nblocks || m_bad >= m_nblocks || m_dmap >= m_nblocks || m_root >= m_nblocks) {
		err = "inconsistent DOS/50 label"; return false;
	}
	return true;
}

std::string volume::signature() const { return std::string(reinterpret_cast<const char *>(blk(LABEL_BLOCK)), 16); }

std::string volume::volume_name() const
{
	const u8 *l = blk(LABEL_BLOCK) + 0x14;
	std::string s;
	for (int i = 0; i < 14 && l[i]; i++) s += char(l[i]);   // up to 14 characters, NUL-terminated (FORMAT.md)
	return s;
}

void volume::set_volume_name(const std::string &n)
{
	u8 *l = blk(LABEL_BLOCK) + 0x14;
	memset(l, 0, 15);
	memcpy(l, n.data(), std::min<size_t>(n.size(), 14));
}

u32 volume::label_time() const { const u8 *l = blk(LABEL_BLOCK); return (u32(rw16(l + 0x26)) << 16) | rw16(l + 0x28); }

// ---------------------------------------------------------------- bitmaps (1 = free)

bool volume::block_free(int b) const { return (blk(m_bmp)[b >> 3] >> (b & 7)) & 1; }
void volume::set_block_free(int b, bool f) { u8 &x = blk(m_bmp)[b >> 3]; x = f ? u8(x | (1 << (b & 7))) : u8(x & ~(1 << (b & 7))); }
// the descriptor map is indexed by block number: id N is bit 6+N
bool volume::id_in_use(int id) const { int s = FIRST_DESC_BLOCK + id; return !((blk(m_dmap)[s >> 3] >> (s & 7)) & 1); }
void volume::set_id_free(int id, bool f) { int s = FIRST_DESC_BLOCK + id; u8 &x = blk(m_dmap)[s >> 3]; x = f ? u8(x | (1 << (s & 7))) : u8(x & ~(1 << (s & 7))); }

int volume::free_blocks() const { int n = 0; for (int b = 0; b < m_nblocks; b++) n += block_free(b); return n; }
int volume::free_files() const { int n = 0; for (int i = 0; i < m_maxfiles; i++) n += !id_in_use(i); return n; }
int volume::bad_blocks() const { int n = 0; for (int b = 0; b < m_nblocks; b++) n += (blk(m_bad)[b >> 3] >> (b & 7)) & 1; return n; }

bool volume::alloc_blocks(int n, std::vector<extent> &out)
{
	out.clear();
	if (n == 0) return true;
	if (free_blocks() < n) return false;
	// first fit, contiguous if possible
	for (int b = 0, run = 0; b < m_nblocks; b++) {
		run = block_free(b) ? run + 1 : 0;
		if (run == n) { out.push_back({ u16(b - n + 1), u16(n) }); break; }
	}
	if (out.empty())
		for (int b = 0; b < m_nblocks && n; b++)
			if (block_free(b)) {
				if (!out.empty() && out.back().block + out.back().count == b) out.back().count++;
				else out.push_back({ u16(b), 1 });
				n--;
			}
	for (auto &e : out) for (int b = e.block; b < e.block + e.count; b++) set_block_free(b, false);
	return true;
}

void volume::free_extents(const std::vector<extent> &e)
{
	for (auto &x : e) for (int b = x.block; b < x.block + x.count && b < m_nblocks; b++) set_block_free(b, true);
}

int volume::alloc_id()
{
	for (int i = 1; i < m_maxfiles; i++)
		if (!id_in_use(i)) { set_id_free(i, false); return i; }
	return -1;
}

// ---------------------------------------------------------------- files and directories

std::vector<u8> volume::read_file(int id)
{
	descriptor d = desc(id);
	std::vector<u8> out;
	for (auto &e : d.extents())
		for (int b = e.block; b < e.block + e.count && b < m_nblocks; b++) { const u8 *p = blk(b); out.insert(out.end(), p, p + BLOCK); }
	if (out.size() > d.size()) out.resize(d.size());
	return out;
}

std::vector<dir_entry> volume::list_dir(int dir_id)
{
	std::vector<dir_entry> v;
	std::vector<u8> data = read_file(dir_id);
	for (size_t o = 0; o + ENTRY <= data.size(); o += ENTRY) {
		int id = rw16(&data[o]);
		std::string name;
		for (int i = 0; i < NAME_LEN && data[o + 2 + i]; i++) name += char(data[o + 2 + i]);
		if (id == 0 && name.empty()) continue;      // free slot (deleted entries are zeroed)
		v.push_back({ int(o / ENTRY), id, name });
	}
	return v;
}

bool volume::name_matches(const std::string &stored, const std::string &want)
{
	if (stored == want) return true;
	size_t lb = stored.find('['), rb = stored.find(']');
	if (lb == std::string::npos || rb == std::string::npos || rb < lb) return false;
	std::string full = stored.substr(0, lb) + stored.substr(lb + 1, rb - lb - 1);
	return want.size() >= lb && want.size() <= full.size() && full.compare(0, want.size(), want) == 0;
}

int volume::find(int dir_id, const std::string &name)
{
	auto ents = list_dir(dir_id);
	for (auto &e : ents) if (e.name == name) return e.id;
	for (auto &e : ents) if (name_matches(e.name, name)) return e.id;
	return -1;
}

static std::vector<std::string> split_path(const std::string &p)
{
	std::vector<std::string> v; std::string cur;
	for (char c : p) { if (c == '/') { if (!cur.empty()) v.push_back(cur); cur.clear(); } else cur += c; }
	if (!cur.empty()) v.push_back(cur);
	return v;
}

int volume::resolve(const std::string &path, std::string &err)
{
	int id = 0;
	for (auto &c : split_path(path)) {
		if (!desc(id).is_dir()) { err = "not a directory on the way to " + path; return -1; }
		id = find(id, c);
		if (id < 0) { err = "file not found: " + path; return -1; }
	}
	return id;
}

int volume::resolve_parent(const std::string &path, std::string &leaf, std::string &err)
{
	auto parts = split_path(path);
	if (parts.empty()) { err = "empty file name"; return -1; }
	leaf = parts.back(); parts.pop_back();
	std::string dir;
	for (auto &p : parts) dir += "/" + p;
	int id = resolve(dir, err);
	if (id >= 0 && !desc(id).is_dir()) { err = dir + " is not a directory"; return -1; }
	return id;
}

// A name may end in one abbreviation pair, as DOS/50's command files do: ATT[RIBUTE] (the
// part before '[' is the shortest accepted form) or DS[] (no abbreviation)
bool volume::valid_name(const std::string &n)
{
	if (n.empty() || n.size() > NAME_LEN) return false;
	const size_t lb = n.find('[');
	if (lb != std::string::npos && (lb == 0 || n.back() != ']' || n.find('[', lb + 1) != std::string::npos)) return false;
	for (size_t i = 0; i < n.size(); i++) {
		const unsigned char c = (unsigned char)n[i];
		if (c <= ' ' || c >= 0x7f || c == '/' || c == '*' || c == '?') return false;
		if ((c == '[' && i != lb) || (c == ']' && (lb == std::string::npos || i != n.size() - 1))) return false;
	}
	return true;
}

bool volume::write_data(descriptor &d, const std::vector<u8> &data, std::string &err)
{
	free_extents(d.extents());
	int n = int((data.size() + BLOCK - 1) / BLOCK);
	std::vector<extent> ex;
	if (!alloc_blocks(n, ex)) { d.set_extents({}); d.set_size(0); d.set_consumption(0); err = "disc full"; return false; }
	if (ex.size() > size_t(MAX_EXTENTS)) { free_extents(ex); d.set_extents({}); d.set_size(0); d.set_consumption(0); err = "file too fragmented"; return false; }
	size_t off = 0;
	for (auto &e : ex)
		for (int b = e.block; b < e.block + e.count; b++) {
			u8 *p = blk(b);
			memset(p, 0, BLOCK);
			size_t k = std::min<size_t>(BLOCK, data.size() - off);
			memcpy(p, data.data() + off, k);
			off += k;
		}
	d.set_extents(ex);
	d.set_size(u32(data.size()));
	d.set_consumption(u16(n));
	return true;
}

static void init_descriptor(descriptor &d, u32 now, const std::string &owner, bool dir)
{
	memset(d.raw(), 0, BLOCK);
	d.set_ctime(now); d.set_mtime(now); d.set_atime(now);
	d.set_caps(CAP_OR | CAP_OW | CAP_PR);     // DOS/50's default: OR/OW/PR
	d.set_flags(1);
	d.set_owner(owner.empty() ? "NO.NAME" : owner);
	d.set_links(1);
	d.set_dir(dir);
	d.raw()[0x1fc] = d.raw()[0x1fd] = 0xff;   // FFFF in every descriptor seen
}

bool volume::add_entry(int dir_id, int id, const std::string &name, std::string &err)
{
	descriptor dd = desc(dir_id);
	std::vector<u8> data = read_file(dir_id);
	size_t slot = data.size() / ENTRY;
	for (size_t o = 0; o + ENTRY <= data.size(); o += ENTRY)
		if (std::all_of(data.begin() + o, data.begin() + o + ENTRY, [](u8 b) { return b == 0; })) { slot = o / ENTRY; break; }
	size_t end = std::max(data.size(), (slot + 1) * ENTRY);
	// grow by one block when the entry falls beyond the allocated blocks
	std::vector<extent> ex = dd.extents();
	if (end > size_t(dd.consumption()) * BLOCK) {
		std::vector<extent> more;
		if (!alloc_blocks(1, more)) { err = "disc full"; return false; }
		memset(blk(more[0].block), 0, BLOCK);
		if (!ex.empty() && ex.back().block + ex.back().count == more[0].block) ex.back().count++;
		else ex.push_back(more[0]);
		if (ex.size() > size_t(MAX_EXTENTS)) { free_extents(more); err = "directory too fragmented"; return false; }
		dd.set_extents(ex);
		dd.set_consumption(u16(dd.consumption() + 1));
	}
	// locate the slot's block through the extent list
	size_t off = slot * ENTRY, bi = off / BLOCK;
	for (auto &e : ex) {
		if (bi < e.count) {
			u8 *p = blk(int(e.block + bi)) + off % BLOCK;
			memset(p, 0, ENTRY);
			ww16(p, u16(id));
			memcpy(p + 2, name.data(), name.size());
			break;
		}
		bi -= e.count;
	}
	dd.set_size(u32(end));
	return true;
}

int volume::create_file(int dir_id, const std::string &name, const std::vector<u8> &data, u32 now, const std::string &owner, std::string &err)
{
	if (!valid_name(name)) { err = "invalid DOS/50 name: " + name; return -1; }
	for (auto &e : list_dir(dir_id)) if (e.name == name) { err = "file exists: " + name; return -1; }
	int id = alloc_id();
	if (id < 0) { err = "no free file descriptors"; return -1; }
	descriptor d = desc(id);
	init_descriptor(d, now, owner, false);
	if (!write_data(d, data, err) || !add_entry(dir_id, id, name, err)) { release(id); return -1; }
	if (data.empty()) d.set_flags(1);
	desc(dir_id).set_mtime(now);
	return id;
}

bool volume::replace_data(int id, const std::vector<u8> &data, u32 now, std::string &err)
{
	descriptor d = desc(id);
	if (d.is_dir()) { err = "is a directory"; return false; }
	if (!write_data(d, data, err)) return false;
	d.set_mtime(now); d.set_atime(now);
	return true;
}

int volume::make_dir(int dir_id, const std::string &name, u32 now, const std::string &owner, std::string &err)
{
	if (!valid_name(name)) { err = "invalid DOS/50 name: " + name; return -1; }
	for (auto &e : list_dir(dir_id)) if (e.name == name) { err = "file exists: " + name; return -1; }
	int id = alloc_id();
	if (id < 0) { err = "no free file descriptors"; return -1; }
	descriptor d = desc(id);
	init_descriptor(d, now, owner, true);      // CREATE makes an empty directory: no blocks, size 0
	if (!add_entry(dir_id, id, name, err)) { release(id); return -1; }
	desc(dir_id).set_mtime(now);
	return id;
}

bool volume::link(int id, int dir_id, const std::string &name, std::string &err)
{
	if (!valid_name(name)) { err = "invalid DOS/50 name: " + name; return false; }
	for (auto &e : list_dir(dir_id)) if (e.name == name) { err = "file exists: " + name; return false; }
	descriptor d = desc(id);
	if (d.links() == 255) { err = "too many links"; return false; }
	if (!add_entry(dir_id, id, name, err)) return false;
	d.set_links(u8(d.links() + 1));
	return true;
}

void volume::release(int id)
{
	descriptor d = desc(id);
	free_extents(d.extents());
	d.set_extents({});
	d.set_size(0); d.set_consumption(0); d.set_flags(0); d.set_links(0);
	set_id_free(id, true);
}

bool volume::unlink(int dir_id, const std::string &name, std::string &err)
{
	descriptor dd = desc(dir_id);
	std::vector<u8> data = read_file(dir_id);
	int slot = -1, id = -1;
	for (auto &e : list_dir(dir_id)) if (e.name == name) { slot = e.slot; id = e.id; break; }
	if (slot < 0) { err = "file not found: " + name; return false; }
	descriptor d = desc(id);
	if (d.is_dir() && d.links() <= 1 && !list_dir(id).empty()) { err = name + " is a directory that is not empty"; return false; }
	// zero the entry in place
	std::vector<extent> ex = dd.extents();
	size_t off = size_t(slot) * ENTRY, bi = off / BLOCK;
	for (auto &e : ex) { if (bi < e.count) { memset(blk(int(e.block + bi)) + off % BLOCK, 0, ENTRY); break; } bi -= e.count; }
	memset(&data[off], 0, ENTRY);
	// trim trailing free entries, and give back blocks no longer needed (as DOS/50 does)
	size_t size = data.size();
	while (size >= ENTRY && std::all_of(data.begin() + (size - ENTRY), data.begin() + size, [](u8 b) { return b == 0; })) size -= ENTRY;
	dd.set_size(u32(size));
	int need = int((size + BLOCK - 1) / BLOCK), have = dd.consumption();
	while (have > need && !ex.empty()) {
		extent &last = ex.back();
		set_block_free(last.block + last.count - 1, true);
		if (--last.count == 0) ex.pop_back();
		have--;
	}
	dd.set_extents(ex);
	dd.set_consumption(u16(have));
	if (d.links() > 1) d.set_links(u8(d.links() - 1));
	else release(id);
	return true;
}

bool volume::rename_entry(int dir_id, const std::string &name, const std::string &newname, std::string &err)
{
	if (!valid_name(newname)) { err = "invalid DOS/50 name: " + newname; return false; }
	auto ents = list_dir(dir_id);
	for (auto &e : ents) if (e.name == newname) { err = "file exists: " + newname; return false; }
	for (auto &e : ents)
		if (e.name == name) {
			descriptor dd = desc(dir_id);
			size_t off = size_t(e.slot) * ENTRY, bi = off / BLOCK;
			for (auto &x : dd.extents()) {
				if (bi < x.count) { u8 *p = blk(int(x.block + bi)) + off % BLOCK; memset(p + 2, 0, NAME_LEN); memcpy(p + 2, newname.data(), newname.size()); break; }
				bi -= x.count;
			}
			return true;
		}
	err = "file not found: " + name;
	return false;
}

check_report volume::check()
{
	check_report r;
	std::map<int, int> refs;                 // id -> directory entries naming it
	std::set<int> seen;
	std::vector<int> todo{ 0 };
	refs[0] = 1;
	while (!todo.empty()) {
		int dir = todo.back(); todo.pop_back();
		if (!seen.insert(dir).second) continue;
		for (auto &e : list_dir(dir)) {
			if (e.id <= 0 || e.id >= m_maxfiles) { r.problems.push_back("entry " + e.name + " has invalid id " + std::to_string(e.id)); continue; }
			refs[e.id]++;
			if (desc(e.id).is_dir()) todo.push_back(e.id);
		}
	}
	std::vector<int> owner(size_t(m_nblocks), -1);
	// live files first: everything about them must be right
	for (int id = 0; id < m_maxfiles; id++) {
		bool used = id_in_use(id), named = refs.count(id) != 0;
		descriptor d = desc(id);
		if (named && !used) r.problems.push_back("file id " + std::to_string(id) + " is named in a directory but marked free");
		if (!named) continue;
		if (id != 0 && d.links() != refs[id]) r.problems.push_back("file id " + std::to_string(id) + ": link count " + std::to_string(d.links()) + ", directory entries " + std::to_string(refs[id]));
		int cons = 0;
		for (auto &e : d.extents())
			for (int b = e.block; b < e.block + e.count; b++, cons++) {
				if (b >= m_nblocks) { r.problems.push_back("file id " + std::to_string(id) + " has a block beyond the volume"); break; }
				if (owner[size_t(b)] >= 0) r.problems.push_back("block " + std::to_string(b) + " shared by ids " + std::to_string(owner[size_t(b)]) + " and " + std::to_string(id));
				owner[size_t(b)] = id;
				if (block_free(b)) r.problems.push_back("block " + std::to_string(b) + " of id " + std::to_string(id) + " is marked free");
			}
		if (cons != d.consumption()) r.problems.push_back("file id " + std::to_string(id) + ": consumption " + std::to_string(d.consumption()) + ", blocks " + std::to_string(cons));
		if (d.size() > u32(cons) * BLOCK) r.problems.push_back("file id " + std::to_string(id) + ": size exceeds its blocks");
	}
	// orphans: descriptors still marked in use that no directory names. Their extent
	// lists can be stale, so they only own blocks still in use and owned by no live file.
	for (int id = 1; id < m_maxfiles; id++) {
		if (!id_in_use(id) || refs.count(id)) continue;
		r.orphans.push_back(id);
		for (auto &e : desc(id).extents())
			for (int b = e.block; b < e.block + e.count && b < m_nblocks; b++)
				if (!block_free(b) && owner[size_t(b)] < 0) { owner[size_t(b)] = id; r.orphan_blocks++; }
	}
	int lost = 0;
	for (int b = m_dmap + 1; b < m_nblocks; b++) if (!block_free(b) && owner[size_t(b)] < 0) lost++;
	if (lost) r.problems.push_back(std::to_string(lost) + " blocks marked in use but owned by no file");
	return r;
}

int volume::free_orphans(const check_report &r)
{
	// free only blocks no live file owns (an orphan's extent list may be stale)
	std::vector<bool> live(size_t(m_nblocks), false);
	std::set<int> orph(r.orphans.begin(), r.orphans.end());
	for (int id = 0; id < m_maxfiles; id++)
		if (id_in_use(id) && !orph.count(id))
			for (auto &e : desc(id).extents())
				for (int b = e.block; b < e.block + e.count && b < m_nblocks; b++) live[size_t(b)] = true;
	int n = 0;
	for (int id : r.orphans) {
		if (id == 0) continue;
		descriptor d = desc(id);
		for (auto &e : d.extents())
			for (int b = e.block; b < e.block + e.count && b < m_nblocks; b++) if (!live[size_t(b)]) set_block_free(b, true);
		d.set_extents({});
		d.set_size(0); d.set_consumption(0); d.set_flags(0); d.set_links(0);
		set_id_free(id, true);
		n++;
	}
	return n;
}

// ---------------------------------------------------------------- new volume

bool volume::create(int maxfiles, const std::string &volname, const std::string &owner, const geometry &g, std::string &err)
{
	int total = g.blocks();
	if (maxfiles < 16 || FIRST_DESC_BLOCK + maxfiles + 4 >= total) { err = "maxfiles out of range for " + g.name(); return false; }
	m_geom = g;
	m_img.assign(size_t(g.records()) * RECORD, 0);
	m_is_imd = false; m_imd_comment.clear(); m_path.clear();
	u8 *l = blk(LABEL_BLOCK);
	memcpy(l, "TEKTRONIX DOS/50", 16);
	u32 now = now_local();
	ww16(l + 0x24, 0x0101);
	ww16(l + 0x26, u16(now >> 16)); ww16(l + 0x28, u16(now));
	int bmp = FIRST_DESC_BLOCK + maxfiles;
	ww16(l + 0x2a, u16(bmp)); ww16(l + 0x2c, u16(bmp + 1)); ww16(l + 0x2e, u16(bmp + 2)); ww16(l + 0x30, u16(bmp + 3));
	ww16(l + 0x32, u16(g.blocks() - 1));             // last block, as FORMAT writes (records/2 - 1)
	ww16(l + 0x34, u16(maxfiles - 1));
	if (!parse_label(err)) return false;
	set_volume_name(volname);
	for (int b = 0; b < m_nblocks; b++) set_block_free(b, b > m_root);
	for (int i = 0; i < m_maxfiles; i++) set_id_free(i, i != 0);
	// as FORMAT leaves it (compare the 8085/8086/PROM distribution discs):
	// root (id 0) with one block, and 8501_BOOTSTRAP (id 1) reserving blocks 0-4
	// for the boot code, size 0 until a system is installed. Both belong to the
	// formatting user: DOS/50 refuses public writes to a root without PW (the
	// distribution discs' roots are TEKTRONIX's, and writing there as NO.NAME
	// gives "Error 12 Device I/O error on write").
	descriptor root = desc(0);
	init_descriptor(root, now, owner, true);
	root.set_extents({ { u16(m_root), 1 } });
	root.set_consumption(1);
	set_id_free(1, false);
	descriptor boot = desc(1);
	init_descriptor(boot, now, owner, false);
	boot.set_extents({ { 0, 5 } });
	boot.set_consumption(5);
	return add_entry(0, 1, "8501_BOOTSTRAP", err);
}

// ---------------------------------------------------------------- formatting

// DOS/50 time: seconds since 1-JAN-70 00:00 (local), on a calendar of twelve
// 31-day months (372-day years). Established by comparing raw descriptor times
// with ATTRIBUTE output: day 4000 = 02-OCT-80, 4468 = 05-JAN-82, 20768 = 30-OCT-25.
std::string volume::format_time(u32 t)
{
	static const char *mon[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
	u32 days = t / 86400, secs = t % 86400;
	u32 year = 1970 + days / 372, m = days % 372 / 31, d = days % 31 + 1;
	char b[32];
	snprintf(b, sizeof b, "%02u-%s-%02u/%02u:%02u:%02u", d, mon[m], year % 100, secs / 3600, secs / 60 % 60, secs % 60);
	return b;
}

u32 volume::encode_time(int year, int month, int day, int h, int mi, int sec)
{
	return u32(((year - 1970) * 372 + (month - 1) * 31 + (day - 1)) * 86400 + h * 3600 + mi * 60 + sec);
}

u32 volume::now_local()
{
	time_t t = time(nullptr);
	struct tm lt;
	localtime_r(&t, &lt);
	return encode_time(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
}

std::string volume::caps_string(u8 c)
{
	std::string s;
	auto add = [&](u8 bit, const char *n) { if (c & bit) { if (!s.empty()) s += "/"; s += n; } };
	add(CAP_OR, "OR"); add(CAP_OW, "OW"); add(CAP_PR, "PR"); add(CAP_PW, "PW");
	return s.empty() ? "-" : s;
}

} // namespace dos50
