// TEKDOS volume (Tektronix 8001/8002/8002A µProcessor Lab, Flexible Disc Unit).
// On-disk format from the FDU service manual 070-2587 Table 1-1, checked on the
// bitsavers disc collection; see FORMAT-TEKDOS.md.
#ifndef TEKDOS_VOLUME_HPP
#define TEKDOS_VOLUME_HPP
#include <cstdint>
#include <string>
#include <vector>

namespace tekdos {

using u8 = uint8_t; using u16 = uint16_t;

constexpr int TRACKS = 77, SECTORS = 32, SECTOR = 128;
constexpr int IMAGE = TRACKS * SECTORS * SECTOR;     // 315,392 bytes, logical sector order
constexpr int BLOCK_SECTORS = 8, BLOCK = BLOCK_SECTORS * SECTOR;
constexpr int BLOCKS = (TRACKS - 1) * SECTORS / BLOCK_SECTORS;   // 304: tracks 1-76
constexpr int SLOTS = 78;                            // directory entries
constexpr int NAME_LEN = 8;
constexpr int SYSTEM_BLOCKS = 16;                    // TEKDOS itself: tracks 1-4 = blocks 0-15
constexpr int IDENT_LEN = 48;                        // disc identifier (FORMAT truncates to 48)

struct entry {
	int slot;
	std::string name;          // as stored, trailing blanks removed ("\x0f" is the system overlay file)
	int blocks, sectors, last; // blocks allocated; sectors used; data bytes in the last sector
	u16 size() const { return u16(sectors ? (sectors - 1) * SECTOR + last : 0); }
};

// a load module's header ("MOD" files: TEKDOS's MODULE command)
struct module_header { bool valid = false; u16 load_lo = 0, load_hi = 0, start = 0; std::string ident; };

struct check_report { std::vector<std::string> problems; };

class volume {
public:
	bool load(const std::string &path, std::string &err);
	bool save(const std::string &path, std::string &err) const;
	void create(const std::string &ident);               // as TEKDOS's FORMAT leaves a disc
	bool loaded() const { return !m_img.empty(); }
	const std::string &path() const { return m_path; }

	std::string ident() const;                            // disc identifier (FORMAT's name)
	void set_ident(const std::string &s);
	int active_count() const { return m_img[2]; }         // sector 0 byte 2
	std::vector<entry> entries() const;
	int find(const std::string &name) const;              // slot, or -1
	std::vector<u8> read_file(int slot) const;
	module_header header(int slot) const;
	int write_file(const std::string &name, const std::vector<u8> &data, std::string &err);   // creates or replaces
	int write_system_overlay(const std::vector<u8> &data, std::string &err);   // the '.' file (slot name 0F)
	bool remove(const std::string &name, std::string &err);
	bool rename(const std::string &from, const std::string &to, std::string &err);
	int free_blocks() const;
	int bad_blocks() const;
	int used_blocks() const;
	check_report check() const;

	static bool valid_name(const std::string &n, std::string &why);
	static std::string display_name(const std::string &n) { return n == "\x0f" ? "." : n; }

private:
	std::vector<u8> m_img;
	std::string m_path;

	u8 *sector(int track, int s) { return &m_img[size_t(track * SECTORS + s) * SECTOR]; }
	const u8 *sector(int track, int s) const { return &m_img[size_t(track * SECTORS + s) * SECTOR]; }
	u8 *name_at(int slot) { return sector(0, 1) + slot * NAME_LEN; }
	const u8 *name_at(int slot) const { return sector(0, 1) + slot * NAME_LEN; }
	u8 *record(int slot) { return sector(0, 6 + slot / 3) + (slot % 3) * 41; }
	const u8 *record(int slot) const { return sector(0, 6 + slot / 3) + (slot % 3) * 41; }
	static size_t block_offset(int b) { return size_t(1 + b / 4) * SECTORS * SECTOR + size_t(b % 4) * BLOCK; }
	static bool bit(const u8 *map, int b) { return (map[b >> 3] >> (b & 7)) & 1; }
	static void set_bit(u8 *map, int b, bool v) { if (v) map[b >> 3] |= u8(1 << (b & 7)); else map[b >> 3] &= u8(~(1 << (b & 7))); }
	u8 *master() { return &m_img[90]; }
	const u8 *master() const { return &m_img[90]; }
	const u8 *badmap() const { return &m_img[52]; }
	std::vector<int> file_blocks(int slot) const;
	int store(const std::string &name, const std::vector<u8> &data, std::string &err);
	bool slot_used(int slot) const;
	void free_slot(int slot);
	void update_count();
};

} // namespace tekdos
#endif
