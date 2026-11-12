// tekmato commands: one class per CLI command, dispatched by tekmato.cpp (the same
// pattern as tomato's command_<name> classes). Commands reach the disc only through
// tekmato::filesystem (filesystem.hpp): none of them knows which file system is open.
#ifndef TEKMATO_COMMAND_HPP
#define TEKMATO_COMMAND_HPP
#include "filesystem.hpp"
#include <iostream>
#include <list>
#include <memory>
#include <string>

extern const char *const TEKMATO_VERSION;
extern const char *const TEKMATO_VERSION_DATE;

class command {
public:
	virtual ~command() = default;
	virtual void help() = 0;
	virtual void process(std::list<std::string> &pars) = 0;
	static int status;                       // 0 ok, 2 an error occurred

protected:
	using bytes = tekmato::bytes;
	static std::unique_ptr<tekmato::filesystem> fs;   // the open volume, shared by every command
	static bool dirty;                        // modified since -open / -save
	static std::string owner;                 // owner for new files (-owner; DOS/50)
	bool need_volume();
	void error(const std::string &msg);
	void info(const std::string &msg) { std::cout << "I : " << msg << std::endl; }
	static bool host_read(const std::string &path, bytes &out, std::string &err);
	static bool host_write(const std::string &path, const bytes &data, std::string &err);
	// both systems end text lines with CR alone
	static void text_to_host(bytes &d);       // CR -> LF
	static void text_to_disc(bytes &d);       // LF / CR LF -> CR
	static bool take_flag(std::list<std::string> &pars, const std::string &flag);
	// one host file onto the volume (-write, -import); a dest "" or ending in '/' takes a name
	// made from the host file's; *id gets the file's id
	static bool put_file(const std::string &src, std::string dest, bool text, std::string &err, int *id = nullptr);
};

#define DECLARE_COMMAND(name) \
	class command_##name : public command { public: void help() override; void process(std::list<std::string> &pars) override; };

DECLARE_COMMAND(open)
DECLARE_COMMAND(new)
DECLARE_COMMAND(save)
DECLARE_COMMAND(saveas)
DECLARE_COMMAND(info)
DECLARE_COMMAND(list)
DECLARE_COMMAND(read)
DECLARE_COMMAND(type)
DECLARE_COMMAND(write)
DECLARE_COMMAND(export)
DECLARE_COMMAND(import)
DECLARE_COMMAND(delete)
DECLARE_COMMAND(mkdir)
DECLARE_COMMAND(link)
DECLARE_COMMAND(move)
DECLARE_COMMAND(attrib)
DECLARE_COMMAND(rename)
DECLARE_COMMAND(owner)
DECLARE_COMMAND(check)
#endif
