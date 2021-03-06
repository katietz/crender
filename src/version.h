#ifndef VERSION_HXX
#define VERSION_HXX

bool valdidate_pkg_version(const char *pkg, const char *ver, const std::string &x);
std::string read_pgk_name(const char *&h);
bool does_version_fit(const char *ver, const char *verspec);

#endif
