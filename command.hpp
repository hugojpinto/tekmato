// tekmato commands: one class per CLI command, dispatched by tekmato.cpp
// (the same pattern as tomato's command_<name> classes).
#ifndef TEKMATO_COMMAND_HPP
#define TEKMATO_COMMAND_HPP
#include "dos50_volume.hpp"
#include "tekdos_volume.hpp"
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
	static dos50::volume vol;                 // the open volume, shared by every command
	static tekdos::volume tvol;               // ... or the open TEKDOS volume (8002 discs)
	static bool tk;                           // a TEKDOS volume is open
	bool dos50_only(const char *what);        // false (with an error) on a TEKDOS volume
	static bool dirty;                        // modified since -open / -save
	static std::string owner;                 // owner for new files (-owner)
	bool need_volume();
	void error(const std::string &msg);
	void info(const std::string &msg) { std::cout << "I : " << msg << std::endl; }
	static bool host_read(const std::string &path, std::vector<dos50::u8> &out, std::string &err);
	static bool host_write(const std::string &path, const std::vector<dos50::u8> &data, std::string &err);
	static void to_text_host(std::vector<dos50::u8> &d);    // CR -> LF
	static void to_text_dos50(std::vector<dos50::u8> &d);   // LF / CR LF -> CR
	static std::string host_name(const std::string &dos50_name);   // ATT[RIBUTE] -> ATTRIBUTE
	static std::string dos50_name(const std::string &host_name);   // legal characters, 14 max (case kept)
	static std::string tekdos_name(const std::string &host_name);  // upper case, legal characters, 8 max
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
