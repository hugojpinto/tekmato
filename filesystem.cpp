// The format registry: the only place that lists the file systems tekmato knows.
// Adding one means an adapter (a filesystem subclass with its open/create
// functions) and its lines here; no command changes.
#include "filesystem.hpp"

namespace tekmato {

const std::vector<format> &formats()
{
	static const std::vector<format> list = {
		{ "dos50-dsdd", "DOS/50", "8550 MDL, 8501 DMU: double sided, double density (the standard disc)", create_dos50 },
		{ "dos50-dssd", "DOS/50", "8550 MDL, 8501 DMU: double sided, single density", create_dos50 },
		{ "dos50-ssdd", "DOS/50", "8550 MDL, 8501 DMU: single sided, double density", create_dos50 },
		{ "dos50-sssd", "DOS/50", "8550 MDL, 8501 DMU: single sided, single density", create_dos50 },
		{ "tkdos-sssd", "TEKDOS", "8001/8002/8002A µProcessor Lab, Flexible Disc Unit: 77 tracks x 32 hard sectors x 128", create_tekdos },
	};
	return list;
}

const format *find_format(const std::string &name)
{
	for (auto &f : formats())
		if (name == f.name || (std::string(f.family) == "DOS/50" && name == f.name + 6)) return &f;   // 0.2: sssd ... dsdd
	return nullptr;
}

std::unique_ptr<filesystem> open_image(const std::string &path, const std::string &owner, std::string &err)
{
	// TEKDOS images have one exact size; DOS/50's loader takes raw images of its four
	// geometries and ImageDisk files, and says why anything else is not one
	if (probe_tekdos(path)) return open_tekdos(path, err);
	return open_dos50(path, owner, err);
}

} // namespace tekmato
