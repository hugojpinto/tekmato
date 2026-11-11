// tekmato -- TEKtronix MAnipulation TOol: DOS/50 disc images of the Tektronix 8550 MDL.
// Same command-line model as tomato: -open an image, chain commands, -save.
#include "command.hpp"
#include <map>

using namespace std;

static map<string, command *> cmd_map = {
	{ "-attrib", new command_attrib() }, { "-check", new command_check() },   { "-delete", new command_delete() },
	{ "-export", new command_export() }, { "-import", new command_import() }, { "-info", new command_info() },
	{ "-link", new command_link() },     { "-list", new command_list() },     { "-mkdir", new command_mkdir() },
	{ "-move", new command_move() },     { "-new", new command_new() },       { "-open", new command_open() },
	{ "-owner", new command_owner() },   { "-read", new command_read() },     { "-rename", new command_rename() },
	{ "-save", new command_save() },     { "-saveas", new command_saveas() }, { "-type", new command_type() },
	{ "-write", new command_write() },
};

static void print_banner() { cerr << endl << "TEKtronix MAnipulation TOol v" << TEKMATO_VERSION << " (" << TEKMATO_VERSION_DATE << ")" << endl << endl; }

static void command_help()
{
	cout << "Usage: tekmato -open <image> [COMMANDS...]" << endl << endl
	     << "Reads, writes and checks DOS/50 disc images of the Tektronix 8550" << endl
	     << "Microcomputer Development Lab: 8501 Data Management Unit floppies in" << endl
	     << "all four formats (SSSD, SSDD, DSSD, DSDD), as raw .img or ImageDisk .imd." << endl
	     << "Paths are '/'-separated from the volume root; changes stay in memory" << endl
	     << "until -save or -saveas." << endl << endl
	     << "Names may end in an abbreviation, as DOS/50's commands do: ATT[RIBUTE]" << endl
	     << "answers to ATT, ATTR, ... ATTRIBUTE, and DS[] only to DS. To refer to an" << endl
	     << "existing file, tekmato accepts any abbreviation (-read 8301/COMMANDS/ATTR);" << endl
	     << "DOS/50 itself does so only for commands, and in a path needs the full name." << endl
	     << "To create one, give the bracketed name in quotes, since brackets are shell" << endl
	     << "wildcards: -write ds.ovl '8301/DS[]'." << endl << endl
	     << "Commands:" << endl;
	for (auto &c : cmd_map) { c.second->help(); cout << endl; }
	cout << "Examples:" << endl
	     << "  tekmato -open System.imd -info -list" << endl
	     << "  tekmato -open System.imd -type 8301/COMMANDS/HELP" << endl
	     << "  tekmato -open work.img -write hello.asm HELLO.ASM text -save" << endl
	     << "  tekmato -new 256 PROJECT -import ./src SRC text -saveas project.img" << endl
	     << "  tekmato -open sys.img -write ds.ovl '8301/8086/DS[]' -save" << endl;
}

int main(int argc, char *argv[])
{
	print_banner();
	list<string> args;
	bool help = argc < 2;
	int bye = help ? 1 : 0;
	for (int i = 1; i < argc; i++) {
		string p = argv[i];
		if (p == "-help" || p == "--help") { help = true; continue; }
		if (p == "-version" || p == "--version") return 0;
		if (p[0] == '-' && p.size() > 1 && !cmd_map.count(p)) {
			cerr << "E : Unknown command: " << p << endl << "    Use -help for a list of available commands." << endl;
			bye = 2;
		}
		args.push_back(p);
	}
	if (help) command_help();
	if (bye || help) return bye;

	string cmd;
	list<string> pars;
	auto run = [&]() {
		if (cmd.empty()) return;
		cout << "C : " << cmd.substr(1) << endl;
		cmd_map[cmd]->process(pars);
	};
	for (auto &p : args) {
		if (p[0] == '-' && p.size() > 1 && cmd_map.count(p)) { run(); cmd = p; pars.clear(); }
		else if (cmd.empty()) { cerr << "E : " << p << ": a command must come first" << endl; return 2; }
		else pars.push_back(p);
	}
	run();
	for (auto &c : cmd_map) delete c.second;
	return command::status;
}
