// Shared state and helpers for the tekmato commands.
#include "command.hpp"
#include <fstream>
#include <iterator>

const char *const TEKMATO_VERSION = "0.3.0";
const char *const TEKMATO_VERSION_DATE = "2026-11-12";

int command::status = 0;
std::unique_ptr<tekmato::filesystem> command::fs;
bool command::dirty = false;
std::string command::owner = "NO.NAME";

bool command::need_volume()
{
	if (fs) return true;
	error("no volume open (use -open FILE or -new first)");
	return false;
}

void command::error(const std::string &msg)
{
	std::cout << "E : " << msg << std::endl;
	status = 2;
}

bool command::host_read(const std::string &path, bytes &out, std::string &err)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) { err = "cannot open " + path; return false; }
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

bool command::host_write(const std::string &path, const bytes &data, std::string &err)
{
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	if (!f) { err = "cannot write " + path; return false; }
	f.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
	if (!f) { err = "write failed: " + path; return false; }
	return true;
}

void command::text_to_host(bytes &d)
{
	for (auto &c : d) if (c == '\r') c = '\n';
}

void command::text_to_disc(bytes &d)
{
	bytes o;
	o.reserve(d.size());
	for (size_t i = 0; i < d.size(); i++) {
		if (d[i] == '\r' && i + 1 < d.size() && d[i + 1] == '\n') continue;
		o.push_back(d[i] == '\n' ? '\r' : d[i]);
	}
	d.swap(o);
}

bool command::take_flag(std::list<std::string> &pars, const std::string &flag)
{
	for (auto it = pars.begin(); it != pars.end(); ++it)
		if (*it == flag) { pars.erase(it); return true; }
	return false;
}

bool command::put_file(const std::string &src, std::string dest, bool text, std::string &err, int *id)
{
	bytes data;
	if (!host_read(src, data, err)) return false;
	if (text) text_to_disc(data);
	if (dest.empty() || dest.back() == '/') dest += fs->name_from_host(src);
	std::string leaf;
	int dir = fs->resolve_parent(dest, leaf, err);
	if (dir < 0) return false;
	int n = fs->write_file(dir, leaf, data, err);
	if (id) *id = n;
	return n >= 0;
}
