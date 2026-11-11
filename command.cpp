// Shared state and helpers for the tekmato commands.
#include "command.hpp"
#include <cctype>
#include <fstream>
#include <iterator>

const char *const TEKMATO_VERSION = "0.1.0";
const char *const TEKMATO_VERSION_DATE = "2026-11-11";

int command::status = 0;
dos50::volume command::vol;
bool command::dirty = false;
std::string command::owner = "NO.NAME";

bool command::need_volume()
{
	if (vol.loaded()) return true;
	error("no volume open (use -open FILE or -new first)");
	return false;
}

void command::error(const std::string &msg)
{
	std::cout << "E : " << msg << std::endl;
	status = 2;
}

bool command::host_read(const std::string &path, std::vector<dos50::u8> &out, std::string &err)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) { err = "cannot open " + path; return false; }
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

bool command::host_write(const std::string &path, const std::vector<dos50::u8> &data, std::string &err)
{
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	if (!f) { err = "cannot write " + path; return false; }
	f.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
	if (!f) { err = "write failed: " + path; return false; }
	return true;
}

// DOS/50 text files end lines with CR alone
void command::to_text_host(std::vector<dos50::u8> &d)
{
	for (auto &c : d) if (c == '\r') c = '\n';
}

void command::to_text_dos50(std::vector<dos50::u8> &d)
{
	std::vector<dos50::u8> o;
	o.reserve(d.size());
	for (size_t i = 0; i < d.size(); i++) {
		if (d[i] == '\r' && i + 1 < d.size() && d[i + 1] == '\n') continue;
		o.push_back(d[i] == '\n' ? '\r' : d[i]);
	}
	d.swap(o);
}

std::string command::host_name(const std::string &n)
{
	std::string s;
	for (char c : n) if (c != '[' && c != ']') s += c;
	return s;
}

std::string command::dos50_name(const std::string &h)
{
	std::string base = h.substr(h.find_last_of('/') + 1), s;
	for (unsigned char c : base) {
		if (s.size() == size_t(dos50::NAME_LEN)) break;
		if (c > ' ' && c < 0x7f && c != '/' && c != '[' && c != ']' && c != '*' && c != '?') s += char(c);
		else if (c == ' ') s += '_';
	}
	return s;
}
