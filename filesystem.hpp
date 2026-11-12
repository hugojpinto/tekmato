// The one interface tekmato's commands work through, whatever is on the disc,
// and the registry of disc formats behind it.
//
// Layers (each knows only the one below):
//   commands (command_*.cpp)    the CLI: parse arguments, move bytes to and from the host
//   filesystem (this file)      what a command can ask of any volume
//   adapters (*_fs.cpp)         one per file system: map the interface onto ...
//   volumes (*_volume.cpp)      ... the on-disk format, which knows nothing of tekmato
//
// A command never asks which file system is open. Operations a file system lacks
// (DOS/50's directories, links and attributes on a TEKDOS disc) have defaults that
// refuse with a message naming the file system; listing, info and check are printed
// by the adapter, because their columns are each system's own.
#ifndef TEKMATO_FILESYSTEM_HPP
#define TEKMATO_FILESYSTEM_HPP
#include <cstdint>
#include <list>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace tekmato {

using bytes = std::vector<uint8_t>;

// a directory entry as commands see it: id is the file's identity (two entries with one
// id are links to one file), unique within the volume; directories are entries too
struct node {
	int id;
	std::string name;        // as stored on the disc
	bool dir;
};

class filesystem {
public:
	virtual ~filesystem() = default;

	// ---- identity and image
	virtual const char *family() const = 0;                  // "DOS/50", "TEKDOS"
	virtual std::string format_name() const = 0;             // "dos50-dsdd", "tkdos-sssd"
	virtual std::string summary() const = 0;                 // one line for -open
	virtual const std::string &path() const = 0;             // file it came from ("" if new)
	virtual bool save(const std::string &path, std::string &err) = 0;
	virtual std::string volume_name() const = 0;
	virtual bool set_volume_name(const std::string &name, std::string &err) = 0;

	// ---- names
	virtual bool valid_name(const std::string &name, std::string &why) const = 0;
	virtual std::string name_from_host(const std::string &host_file) const = 0;   // a legal default name
	virtual std::string host_name(const std::string &name) const { return name; }  // name of an exported file

	// ---- tree ('/'-separated paths from the root)
	virtual int root() const = 0;
	virtual std::vector<node> list_dir(int dir) = 0;
	virtual bool is_dir(int id) = 0;
	virtual int resolve(const std::string &path, std::string &err) = 0;
	virtual int resolve_parent(const std::string &path, std::string &leaf, std::string &err) = 0;
	virtual int find(int dir, const std::string &name) = 0;  // id or -1

	// ---- files
	virtual bytes read_file(int id) = 0;
	virtual int write_file(int dir, const std::string &name, const bytes &data, std::string &err) = 0;   // creates or replaces; id or -1
	virtual bool remove(int dir, const std::string &name, std::string &err) = 0;   // as the system's DELETE (directories with their contents)
	virtual bool rename(int dir, const std::string &name, int new_dir, const std::string &new_name, std::string &err) = 0;

	// ---- what only some systems have
	virtual int make_dir(int dir, const std::string &name, std::string &err) { (void)dir; (void)name; return refuse("directories", err), -1; }
	virtual bool link(int id, int dir, const std::string &name, std::string &err) { (void)id; (void)dir; (void)name; return refuse("links", err); }
	virtual bool attributes(int id, const std::string &path, const std::list<std::string> &changes, std::ostream &out, std::string &err) {
		(void)id; (void)path; (void)changes; (void)out; return refuse("file attributes", err);
	}
	virtual bool set_owner(const std::string &owner, std::string &err) { (void)owner; return refuse("owners", err); }

	// ---- reports (each system's own columns)
	virtual void print_info(std::ostream &out) = 0;
	virtual void print_list(std::ostream &out, int dir, bool recurse, bool ids) = 0;
	virtual bool check(std::ostream &out, bool fix) = 0;     // true: consistent (prints W : lines otherwise)

protected:
	bool refuse(const char *what, std::string &err) const { err = std::string(family()) + " discs have no " + what; return false; }
};

// ---- the formats

struct new_volume {
	int maxfiles = -1;           // -1: the format's default
	std::string name, owner;
};

struct format {
	const char *name;            // as -new takes it
	const char *family;
	const char *description;     // one line of -help
	std::unique_ptr<filesystem> (*create)(const char *format_name, const new_volume &v, std::string &err);
};

const std::vector<format> &formats();
const format *find_format(const std::string &name);     // also the names tekmato 0.2 used (sssd ...)
// open an image of any known format: each family's probe decides from the file
std::unique_ptr<filesystem> open_image(const std::string &path, const std::string &owner, std::string &err);

// one per family, in its adapter
std::unique_ptr<filesystem> open_dos50(const std::string &path, const std::string &owner, std::string &err);
std::unique_ptr<filesystem> create_dos50(const char *format_name, const new_volume &v, std::string &err);
bool probe_tekdos(const std::string &path);
std::unique_ptr<filesystem> open_tekdos(const std::string &path, std::string &err);
std::unique_ptr<filesystem> create_tekdos(const char *format_name, const new_volume &v, std::string &err);

} // namespace tekmato
#endif
