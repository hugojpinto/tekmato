// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Shared state and helpers for the tekmato commands.
#include "command.hpp"
#include <cstring>
#include <fstream>
#include <iterator>

#ifndef TEKMATO_VERSION_STRING
#define TEKMATO_VERSION_STRING "unknown"
#endif
const char *const TEKMATO_VERSION = TEKMATO_VERSION_STRING;   // the VERSION file, from the Makefile

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

bool command::host_read(const std::filesystem::path &path, bytes &out, std::string &err)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) { err = "cannot open " + path.u8string(); return false; }
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

bool command::host_write(const std::filesystem::path &path, const bytes &data, std::string &err)
{
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	if (!f) { err = "cannot write " + path.u8string(); return false; }
	f.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
	if (!f) { err = "write failed: " + path.u8string(); return false; }
	return true;
}

std::string command::host_name_out(const std::string &n)
{
#ifdef _WIN32
	std::string o;
	for (size_t i = 0; i < n.size(); i++) {
		const unsigned char c = static_cast<unsigned char>(n[i]);
		const bool last = i + 1 == n.size();
		if (c < 0x20 || strchr("\"*:<>?\\|", c) || (last && (c == '.' || c == ' ')) ) {
			const unsigned u = 0xF000 + c;          // three UTF-8 bytes: EF 80..81 80..BF
			o += char(0xE0 | (u >> 12)); o += char(0x80 | ((u >> 6) & 0x3F)); o += char(0x80 | (u & 0x3F));
		} else o += char(c);
	}
	return o;
#else
	return n;
#endif
}

std::string command::host_name_in(const std::string &h)
{
	std::string o;
	for (size_t i = 0; i < h.size(); i++) {
		const unsigned char a = static_cast<unsigned char>(h[i]);
		if (a == 0xEF && i + 2 < h.size() && (static_cast<unsigned char>(h[i + 1]) & 0xFE) == 0x80 && (static_cast<unsigned char>(h[i + 2]) & 0xC0) == 0x80) {
			o += char(((static_cast<unsigned char>(h[i + 1]) & 0x3F) << 6) | (static_cast<unsigned char>(h[i + 2]) & 0x3F));
			i += 2;
		} else o += char(a);
	}
	return o;
}

std::filesystem::path command::host_file(const std::string &disc_name)
{
	return std::filesystem::u8path(host_name_out(disc_name));
}

std::string command::disc_name_for(const std::filesystem::path &src)
{
	return fs->name_from_host(host_name_in(src.filename().u8string()));
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

bool command::put_file(const std::filesystem::path &src, std::string dest, bool text, std::string &err, int *id)
{
	bytes data;
	if (!host_read(src, data, err)) return false;
	if (text) text_to_disc(data);
	if (dest.empty() || dest.back() == '/') dest += disc_name_for(src);
	std::string leaf;
	int dir = fs->resolve_parent(dest, leaf, err);
	if (dir < 0) return false;
	int n = fs->write_file(dir, leaf, data, err);
	if (id) *id = n;
	return n >= 0;
}
