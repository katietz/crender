#ifndef YAML_HXX
#define YAML_HXX

#include <string>
#include "yaml-cpp/yaml.h"

#define CRENDER_VERSION "0.1"

bool cr_read_file(const char *fname, std::string &fcontent, bool do_preprocess = true, bool do_jinja2 = true);

void cr_dump_yaml(const YAML::Node &n);
bool cr_has_skip(const YAML::Node &pkg);
bool cr_read_config(const char *fname, bool reset, const char *subn = "cbc");
const YAML::Node &cr_get_config(void);
const YAML::Node cr_find_config(const char *name);
std::string cr_get_config_str(const char *name, int idx = 0, const char *def = NULL);
std::string cr_get_cbc_str(const char *name, const char *def = NULL);
std::string cr_get_cfg_str(const char *name, const char *def = NULL);
std::string cr_get_config_str(const YAML::Node &n, int idx = 0, const char *def = NULL);
bool cr_read_meta(const char *fname);
YAML::Node cr_read_yaml(const char *fname);
bool cr_output_yaml_to_file(const YAML::Node &n, const char *fname);
void info_outputfiles_read(const std::string &fname_prefix);

void cr_set_cfg_var(const char *name, const char *value);
void yaml_set_cbc_x_as(const char *x, const char *as);
void yaml_set_cbc_python_as(const char *py);
void cr_set_cfg_preset(const char *py, const char *numpy);
void check_for_package_version(const std::string &fname);

std::string expand_compiler_int(const char *lang, bool add_version);
std::string expand_compiler(const std::string &lang);

void info_outputfiles(const std::string &fname_prefix);
void info_add_output_2_pkg(const char *oname, const char *pkgname);
void info_add_pkg_2_output(const char *pkgname, const char *version, const char *oname);
void info_add_used_by_pkg(const char *pkname, const char *ver, const char *dep);
void info_add_pkg_version(const char *pkgname, const char *version);
void info_add_depends_on_pkg(const char *pkname, const char *nver, const char *dep);
void add_info_note(const char *pkg, const char *ver, const char *note);
void add_info_url(const char *url, const char *url_kind, const char *pkg, const char *ver, const char *crc_kind, const char *crc);

#endif

