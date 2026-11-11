// DOS/50 volume (Tektronix 8550 MDL, 8501 Data Management Unit flexible disc).
// On-disk format established by reverse engineering; see FORMAT.md.
#ifndef DOS50_VOLUME_HPP
#define DOS50_VOLUME_HPP
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace dos50 {

using u8 = uint8_t; using u16 = uint16_t; using u32 = uint32_t;

constexpr int RECORD = 256;           // controller record
constexpr int BLOCK = 512;            // file system block = 2 records
constexpr int FIRST_DESC_BLOCK = 6;   // descriptor of file id N is block 6+N
constexpr int LABEL_BLOCK = 5;        // record 10
constexpr int ENTRY = 16;             // directory entry: id (2) + name (14)
constexpr int NAME_LEN = 14;
constexpr int MAX_EXTENTS = 0x1b0 / 4;

// capability bits (descriptor 0x1c6), as ATTRIBUTE shows them
constexpr u8 CAP_OR = 0x01, CAP_OW = 0x02, CAP_PR = 0x10, CAP_PW = 0x20;

// 8501 disc geometry: 77 cylinders, 1 or 2 sides, 26 sectors per track; single
// density (FM, 128-byte sectors = 13 records per track) or double density (MFM,
// 256-byte sectors = 26 records), with track 0 always FM. Logical track t is
// cylinder t/2, side t%2 on two-sided discs. Capacities differ, so the record
// count identifies the geometry: SSSD 1001, SSDD 1989, DSSD 2002, DSDD 3991.
struct geometry {
	bool ds = true, dd = true;
	int  tracks() const { return ds ? 154 : 77; }
	bool fm(int t) const { return !dd || t == 0; }
	int  track_first(int t) const { return t == 0 ? 0 : dd ? 13 + 26 * (t - 1) : 13 * t; }
	int  records() const { return track_first(tracks()); }
	int  blocks() const { return records() / 2; }     // FORMAT: last block = records/2 - 1
	int  cylinder(int t) const { return ds ? t / 2 : t; }
	int  side(int t) const { return ds ? t % 2 : 0; }
	std::string name() const { return std::string(ds ? "DS" : "SS") + (dd ? "DD" : "SD"); }
	static bool from_records(size_t n, geometry &g);
	static bool from_name(const std::string &n, geometry &g);
};

struct extent { u16 block, count; };

struct dir_entry {
	int slot;          // index in the directory
	int id;            // file id (descriptor)
	std::string name;  // as stored, e.g. "ATT[RIBUTE]"
};

// A view of one 512-byte file descriptor.
class descriptor {
public:
	descriptor(u8 *p, int id) : m_p(p), m_id(id) {}
	int id() const { return m_id; }
	std::vector<extent> extents() const;
	void set_extents(const std::vector<extent> &e);
	u32 ctime() const { return l32(0x1b4); }
	u32 mtime() const { return l32(0x1b8); }
	u32 atime() const { return l32(0x1bc); }
	u32 size() const { return l32(0x1c0); }
	u16 consumption() const { return w16(0x1c4); }
	u8  caps() const { return m_p[0x1c6]; }
	u8  flags() const { return m_p[0x1c7]; }
	std::string owner() const;
	u8  links() const { return m_p[0x1f8]; }
	bool is_dir() const { return m_p[0x1f9] == 1; }
	void set_ctime(u32 v) { s32(0x1b4, v); }
	void set_mtime(u32 v) { s32(0x1b8, v); }
	void set_atime(u32 v) { s32(0x1bc, v); }
	void set_size(u32 v) { s32(0x1c0, v); }
	void set_consumption(u16 v) { m_p[0x1c4] = u8(v); m_p[0x1c5] = u8(v >> 8); }
	void set_caps(u8 v) { m_p[0x1c6] = v; }
	void set_flags(u8 v) { m_p[0x1c7] = v; }
	void set_owner(const std::string &o);
	void set_links(u8 v) { m_p[0x1f8] = v; }
	void set_dir(bool d) { m_p[0x1f9] = d ? 1 : 0; }
	u8 *raw() { return m_p; }
private:
	u8 *m_p; int m_id;
	u16 w16(int o) const { return u16(m_p[o] | (m_p[o + 1] << 8)); }
	u32 l32(int o) const { return (u32(w16(o)) << 16) | w16(o + 2); }   // PDP-11 order: high word first
	void s32(int o, u32 v) { m_p[o] = u8(v >> 16); m_p[o + 1] = u8(v >> 24); m_p[o + 2] = u8(v); m_p[o + 3] = u8(v >> 8); }
};

struct check_report {
	std::vector<std::string> problems;
	std::vector<int> orphans;          // descriptors in use but reachable from no directory
	int orphan_blocks = 0;
};

class volume {
public:
	// ---- image I/O ----
	bool load(const std::string &path, std::string &err);
	bool save(const std::string &path, std::string &err) const;   // .imd -> IMD, else raw
	bool create(int maxfiles, const std::string &volname, const std::string &owner, const geometry &g, std::string &err);
	const geometry &geom() const { return m_geom; }
	bool loaded() const { return !m_img.empty(); }
	const std::string &path() const { return m_path; }

	// ---- label ----
	std::string signature() const;     // "TEKTRONIX DOS/50"
	std::string volume_name() const;
	void set_volume_name(const std::string &n);
	u32 label_time() const;
	int nblocks() const { return m_nblocks; }
	int maxfiles() const { return m_maxfiles; }
	int bitmap_block() const { return m_bmp; }
	int badmap_block() const { return m_bad; }
	int descmap_block() const { return m_dmap; }
	int root_block() const { return m_root; }
	int free_blocks() const;
	int free_files() const;
	int bad_blocks() const;

	// ---- files ----
	descriptor desc(int id) { return descriptor(blk(FIRST_DESC_BLOCK + id), id); }
	bool id_in_use(int id) const;
	std::vector<u8> read_file(int id);
	std::vector<dir_entry> list_dir(int dir_id);
	// path: '/'-separated from the volume root; abbreviations ("ATT" for "ATT[RIBUTE]") match
	int resolve(const std::string &path, std::string &err);
	int resolve_parent(const std::string &path, std::string &leaf, std::string &err);
	int find(int dir_id, const std::string &name);

	int create_file(int dir_id, const std::string &name, const std::vector<u8> &data, u32 now, const std::string &owner, std::string &err);
	bool replace_data(int id, const std::vector<u8> &data, u32 now, std::string &err);
	int make_dir(int dir_id, const std::string &name, u32 now, const std::string &owner, std::string &err);
	bool link(int id, int dir_id, const std::string &name, std::string &err);
	bool unlink(int dir_id, const std::string &name, std::string &err);   // deletes the file at link count 0
	bool rename_entry(int dir_id, const std::string &name, const std::string &newname, std::string &err);
	check_report check();
	int free_orphans(const check_report &r);

	static bool valid_name(const std::string &n);
	static bool name_matches(const std::string &stored, const std::string &want);
	static std::string format_time(u32 t);    // DD-MMM-YY/HH:MM:SS as DOS/50 shows it
	static u32 now_local();                   // the host clock in DOS/50 time
	static u32 encode_time(int year, int month, int day, int h, int mi, int sec);
	static std::string caps_string(u8 caps);

private:
	std::vector<u8> m_img;
	std::string m_path, m_imd_comment;
	bool m_is_imd = false;
	geometry m_geom;
	int m_nblocks = 0, m_maxfiles = 0, m_bmp = 0, m_bad = 0, m_dmap = 0, m_root = 0;

	u8 *blk(int b) { return &m_img[size_t(b) * BLOCK]; }
	const u8 *blk(int b) const { return &m_img[size_t(b) * BLOCK]; }
	bool parse_label(std::string &err);
	bool block_free(int b) const;
	void set_block_free(int b, bool f);
	void set_id_free(int id, bool f);
	bool alloc_blocks(int n, std::vector<extent> &out);
	void free_extents(const std::vector<extent> &e);
	int alloc_id();
	bool write_data(descriptor &d, const std::vector<u8> &data, std::string &err);
	bool add_entry(int dir_id, int id, const std::string &name, std::string &err);
	void release(int id);
};

} // namespace dos50
#endif
