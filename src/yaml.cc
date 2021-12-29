#include <cstring>
#include <iostream>
#include <string>
#include <cstdlib>
#include <jinja2cpp/template.h>
#include <jinja2cpp/user_callable.h>
#include <jinja2cpp/generic_list_iterator.h>
#include "yaml.h"
#include "prepro.h"
#include "version.h"

// forwarder ...
static std::string cr_get_config_str1(char *, const YAML::Node &, int idx, const char*);
static void add_cbc_values2jinj2(jinja2::ValuesMap &params);
std::string cr_get_cfgcbc_str(const char *name, const char *def);

// external variables
extern bool ignore_skip;

// global variables
bool do_dump_after_readfile = false;

YAML::Node theConfig;

// check if build section in PKG contains a skip
bool cr_has_skip(const YAML::Node &pkg)
{
  if ( !pkg.IsDefined() || !pkg["build"].IsDefined())
     return false;
  const YAML::Node &n = pkg["build"]["skip"];
  if ( !n.IsDefined())
     return false;
  std::string nstr = cr_get_config_str(n, 0, "");

  return (nstr == "true" || nstr == "True" || nstr == "1");
}

std::string apply_min_pin(const std::string &ver, const std::string min_pin)
{
  std::string r = "";
  const char *v = ver.c_str();
  const char *mp = min_pin.c_str();
  if ( *mp == 0)
    return ver;
  while (*mp == 'x' && *v != 0)
  {
    while (*v != 0 && *v != '.') r += *v++;
    mp++;
    if (*mp == '.' ) mp++;
    if (*v == '.') v++;
    if (*v != 0 && *mp != 0) r += ".";
  }
  if ( *mp == 0)
  {
    return ver;
  }
  if (*mp!=0)
    r += ".*";
  return r;
}

std::string apply_max_pin(const std::string &ver, const std::string max_pin)
{
  std::string r = "";
  const char *v = ver.c_str();
  if (*v == 0)
    return r;
  const char *mp = max_pin.c_str();
  if (*mp == 0 )
    return r;
  while (*mp == 'x')
  {
    mp++;
    while (*v != 0 && *v != '.') r += *v++;
    if ( *v == '.' ) v++;
    if (*mp == '.') mp++;
    if (*mp != 0  && *v != 0) r += ".";
  }
  return r;
}

std::string expand_os_environ_get(const std::string &name, const std::string &def)
{
  //std::cerr << "Environment asked for ," << name << "' with default ," << def << "'" << std::endl;
  if (!name.empty())
  {
      const char *envv = getenv(name.c_str());
      if ( envv != NULL )
        return envv;
  }
  return def;
}

std::string expand_pin_subpackage(const std::string &name, const std::string &min_pin, const std::string &max_pin, bool exact)
{
  // we need to get pinning from cbc ....
  std::string pinver = cr_get_cbc_str(name.c_str(), "");
  std::string rslt = name;
  if ( pinver != "" )
  {
    // std::cerr << "pinver: " << pinver << std::endl;
    pinver = apply_max_pin(pinver, max_pin);
    // std::cerr << "pinver: max " << pinver << " " << max_pin << std::endl;
    pinver = apply_min_pin(pinver, min_pin);
    // std::cerr << "pinver: min " << pinver << " " << min_pin << std::endl;
    if ( exact )
      rslt = rslt + " " + pinver;
    else
      rslt = rslt + " " + pinver;
  }
  return rslt;
}


std::string expand_cdt(const std::string &package_name)
{
  std::string cdt_name;
  std::string cdt_arch;
  std::string arch = cr_get_cfg_str("host_arch", "");
  if ( arch == "" )
    arch = cr_get_cfg_str("arch", "64");
  if ( arch == "ppc64le" || arch == "aarch64" || arch == "ppc64" || arch == "s390x")
  {
    cdt_name = "cos7";
    cdt_arch = arch;
  }
  else
  {
    cdt_arch = (arch == "64" ? "x86_64" : "x86");
    cdt_name = "cos6";
  }
  cdt_name = cr_get_cbc_str("cdt_name", cdt_name.c_str());
  cdt_arch = cr_get_cbc_str("cdt_arch", cdt_arch.c_str());
  // name = package_name cut at first space
  if (package_name.length() < 1)
    return std::string("illegal-cdt");
  const char *n = package_name.c_str();
  std::string ver, name;
  name = read_pgk_name(n);
  while (*n == ' ') n++;
  if (*n != 0)
  {
    ver = " ";
    ver += n;
  }

  std::string rslt = name + "-" + cdt_name + "-" + cdt_arch + ver;
  return rslt;
}

std::string expand_compiler_int(const char *lang, bool add_version)
{
  std::string platform = cr_get_cfg_str("platform", "win"); // "linux" "osx"
  std::string ncompiler = "lang_";
  ncompiler += lang;
  ncompiler = cr_get_cfg_str(ncompiler.c_str(), "vs2017");
  std::string version = "";
  std::string r = lang;
  r += "_compiler";
  std::string compiler = cr_get_cbc_str(r.c_str(), "");
  if ( compiler == "" )
    compiler = ncompiler;
  r += "_version";
  version = cr_get_cbc_str(r.c_str(), "");
  std::string target_platform = cr_get_cfg_str("target_platform", platform.c_str());
  compiler += "_" + target_platform;
  if ( add_version && version != "" )
    compiler += " " + version;
  return compiler;
}

std::string expand_compiler(const std::string &lang)
{
  return expand_compiler_int(lang.c_str(), true);
}

std::string expand_ccache(const std::string &method)
{
  // cr_set_cfg_str(ccache_method", method);
  return "ccache";
}


bool cr_read_file(const char *fname, std::string &fcontent, bool do_preprocess, bool do_jinja2)
{
  std::string src;
  if ( !rdtext(fname, src) )
    return false;
#if 0
  std::cout << "original: " << src << std::endl;
#endif

  std::string psrc;
  if ( do_preprocess )
  {
    preprocess_lines(src, psrc);
  }
  else
  {
    psrc = src;
  }

#if 0
  std::cout << "preprocessed: " << psrc << std::endl;
#endif

  if ( !do_jinja2 )
  {
    fcontent = psrc;
    return true;
  }

  bool is_unix = cr_get_cfg_str("unix","0") == "1";
  bool is_win = cr_get_cfg_str("win","0") == "1";

  jinja2::ValuesMap params = {
    {"unix", is_unix},
    {"win",  is_win},
    {"PYTHON", cr_get_cfg_str("PYTHON", is_win ? "%PYTHON%" : "${PYTHON}")},
    {"py", atoi(cr_get_cfg_str("py", "39").c_str())},
    {"py3k", cr_get_cfg_str("py3k", "0") == "1"},
    {"py2k", cr_get_cfg_str("py3k", "0") == "1"},
    {"build_platform", cr_get_cfg_str("target-platform", "win-64")},
    {"target_platform", cr_get_cfg_str("target_platform", "win-64")},
    {"ctng_target_platform", cr_get_cfg_str("target_platform", "win-64")},
    {"ctng_gcc", cr_get_cfg_str("c_compiler_version", "7.3.0")},
    {"ctng_binutils", cr_get_cfg_str("c_compiler_version", "2.35")},
    {"numpy", cr_get_cfgcbc_str("numpy", "1.16")},
    {"np", cr_get_cfg_str("np", "116")},
    {"pl", cr_get_cfg_str("pl", "5")},
    {"lua", cr_get_cfg_str("lua", "5")},
    {"luajit", cr_get_cfg_str("lua", "5")[0] == '2'},
    {"linux64", cr_get_cfg_str("linux-64", "0") == "1"},
    {"aarch64", cr_get_cfg_str("aarch64", "0") == "1"},
    {"ppcle64", cr_get_cfg_str("ppcle64", "0") == "1"},
  };
  add_cbc_values2jinj2(params);

  params["compiler"] = jinja2::MakeCallable(expand_compiler,
      jinja2::ArgInfo{ "lang" });
  params["cdt"] = jinja2::MakeCallable(expand_cdt,
      jinja2::ArgInfo{ "name" });
  params["pin_subpackage"] = jinja2::MakeCallable(expand_pin_subpackage,
      jinja2::ArgInfo{ "subpackage_name" },
      jinja2::ArgInfo{ "min_pin", false, "x.x.x.x.x.x" },
      jinja2::ArgInfo{ "max_pin", false, "x" },
      jinja2::ArgInfoT<bool>{ "exact", false, 0});
  params["pin_compatible"] = jinja2::MakeCallable(expand_pin_subpackage,
      jinja2::ArgInfo{ "subpackage_name" },
      jinja2::ArgInfo{ "min_pin", false, "x.x.x.x.x.x" },
      jinja2::ArgInfo{ "max_pin", false, "x" },
      jinja2::ArgInfoT<bool>{ "exact", false, 0 });
  params["ccache"] = jinja2::MakeCallable(expand_ccache,
      jinja2::ArgInfo{ "method", false, "none" });
  // base python conversion routines
  // os environ
  params["os.environ.get"] = jinja2::MakeCallable(expand_os_environ_get,
      jinja2::ArgInfo{ "name" },
      jinja2::ArgInfo{ "def", false, "" });

  jinja2::Template tpl;
  const auto& pp = tpl.Load(psrc, std::string(fname));
  if ( !pp.has_value() )
  {
    msg_error("after load %s\n", pp.error().ToString().c_str() );
    return false;
  }

  try {
    fcontent = tpl.RenderAsString(params).value();
  } catch(std::exception& ex)
  {
    std::string m = ex.what();
    msg_error("jinja2 exception %s\n", m.c_str());
    return false;
  }
#if 0
  std::cout << "after jinja2: " << fcontent << std::endl;
#endif
  //jinja2::CreateFilter("join")
  return true;
}

bool cr_read_config(const char *fname, bool reset, const char *subn)
{
  if ( !subn || *subn == 0)
    subn = "cbc";
  if ( reset )
  {
    YAML::Node emtyn;
    theConfig = emtyn;
  }
  std::string src;
  if ( !cr_read_file(fname, src) )
    return false;
  YAML::Node n = YAML::Load(src);
  if (n.size() != 0 )
  {
    if ( theConfig.size() == 0  || theConfig[subn].size() == 0)
      theConfig[subn] = n;
    else
    {
      auto it = n.begin();
      while ( it != n.end() )
      {
        std::string s1 = it->first.as<std::string>();
        // std::cout << "added " << s1 << std::endl;
        if ( theConfig[subn][s1].IsDefined() )
          theConfig[subn].remove(s1);
        theConfig[subn][s1] = it->second;
        it++;
      }
    }
  }

  return true;
}

YAML::Node cr_read_yaml(const char *fname)
{
  YAML::Node n;
  std::string src;
  if ( !cr_read_file(fname, src) )
    return n;
  n = YAML::Load(src);
  return n;
}

bool cr_read_meta(const char *fname)
{
  // intialize globals per run for linter

  std::string src;
  if ( !cr_read_file(fname, src) )
    return false;
  if (do_dump_after_readfile)
    msg_warn("File after parsing is:\n%s\n", src.c_str());

  YAML::Node n;
  try {
    n = YAML::Load(src);
  } catch(std::exception& ex)
  {
    std::string m = ex.what();
    msg_error("YAML exception %s\n", m.c_str());
    msg_warn("File after parsing is:\n%s\n", src.c_str());
    return false;
  }
  if (n.size() )
  {
    theConfig["package"] = n;
    // just fill statistic information, if feedstock gets produced
    if (ignore_skip || !cr_has_skip(n))
    {
      check_for_package_version(fname);
    }
  }
  // cr_dump_yaml(theConfig);
  return true;
}

const YAML::Node &cr_get_config(void)
{
  return theConfig;
}

const YAML::Node cr_find_config(const char *name)
{
  const YAML::Node &it = theConfig[name];
  return it;
}

std::string cr_get_config_str(const char *name, int idx, const char *def)
{
  char s[strlen(name)+1];
  strcpy(s, name);
  return cr_get_config_str1(&s[0], theConfig, idx, def);
}

std::string cr_get_cfgcbc_str(const char *name, const char *def)
{
  std::string r = cr_get_cbc_str(name, "");
  if (r.empty())
    r = cr_get_cfg_str(name, def);
  return r;
}

std::string cr_get_cfg_str(const char *name, const char *def)
{
  char s[strlen(name)+1];
  strcpy(s, name);
  return cr_get_config_str1(&s[0], theConfig["config"], 0, def);
}

std::string cr_get_cbc_str(const char *name, const char *def)
{
  char *s = (char*) alloca(strlen(name)+1);
  strcpy(s, name);
  return cr_get_config_str1(&s[0], theConfig["cbc"], 0, def);
}

static std::string cr_get_config_str1(char *name, const YAML::Node &n, int idx, const char *def)
{
  if ( !n.IsDefined() )
    return def ? def : "";

  if (name == NULL || *name == 0)
    return cr_get_config_str(n, idx, def);

  char *h = strchr(name, ':');
  if ( h != NULL )
    *h++ = 0;
  if ( h != NULL && *h >= '0' && *h <= '9')
  {
    char *sv = h;
    h = strchr(name, ':');
    if ( h != NULL)
      *h++ = 0;
    idx = atoi(sv);
  }
  if (!h)
    h = name+strlen(name);
 // cr_dump_yaml(n);
  //std::cerr << "Enter into " << name << std::endl;
  std::string hh = name;
  return cr_get_config_str1(h, n[hh], idx, def);
}

std::string cr_get_config_str(const YAML::Node &n, int idx, const char *def)
{
  if ( !n.IsDefined() )
    return def ? def : "";

  if (n.IsSequence() && n.size() > 0)
    return cr_get_config_str(n[idx], 0, def);
  else if (n.IsScalar())
    return n.as<std::string>();
  else if (n.IsNull())
    return "";
  else if (n.IsMap() && n.size() > 0)
  {
    YAML::Node::const_iterator it = n.begin();
    while (idx > 0) { ++it; --idx; }
    return cr_get_config_str(it->second, 0, def);
  }
  else
  {
    std::cout << "Unhandled type " << n.Type() << std::endl;
    cr_dump_yaml(n);
  }
  return def ? def : "";
}

bool cr_output_yaml_to_file(const YAML::Node &n, const char *fname)
{
  YAML::Emitter em;
  em << n;
  std::string txt = em.c_str();

  if ( !strcmp(fname, "-") )
  {
    std::cout << txt << std::endl;
    return true;
  }
  FILE *fp= fopen(fname, "wb");
  if (!fp)
    return false;
  fwrite(txt.c_str(), 1, strlen(txt.c_str()), fp);
  fclose(fp);
  return true;
}

// dump content of a yaml tree to stderr
void cr_dump_yaml(const YAML::Node &n)
{
  YAML::Emitter em;
  em << n;
  msg_warn("Node YAML is: %s\n", em.c_str());
}

void cr_set_cfg_var(const char *name, const char *val)
{
  if (theConfig["config"].IsDefined() && theConfig["config"][name].IsDefined())
    theConfig["config"].remove(name);
  if (val == NULL || *val == 0)
    return;
  YAML::Node n;
  n.push_back(val);
  theConfig["config"][name] = n;
}


void yaml_set_cbc_x_as(const char *x, const char *as)
{
  if (!x || *x == 0)
    return;
  // remove old key
  if (theConfig["cbc"][x].IsDefined())
      theConfig["cbc"].remove(x);
  // add new key
  if (as && *as != 0)
    theConfig["cbc"][x][0] = YAML::Node(as);
}

void yaml_set_cbc_python_as(const char *py)
{
  yaml_set_cbc_x_as("python", py);
}

static void add_cbc_values2jinj2(jinja2::ValuesMap &params)
{
  const YAML::Node &x = theConfig["cbc"];
  if (!x.IsDefined() || !x.IsMap() || x.size() == 0)
    return;
  YAML::Node::const_iterator it = x.begin();
  for (size_t i = 0; i < x.size(); i++, it++)
  {
    std::string fi = cr_get_config_str(it->first, 0, "");
    std::string cfg = cr_get_cfg_str(fi.c_str(), "");
    const YAML::Node &xr = it->second;
    if (cfg.empty())
      cfg = cr_get_config_str(xr, 0, "");
    if (!cfg.empty())
      params[fi.c_str()] = cfg.c_str();
  }
}

// set the temporary default values in data/cbc
void cr_set_cfg_preset(const char *py, const char *numpy)
{
  std::string vnp;
  if (!py || *py == 0)
    py ="39";
  int vpy = atoi(py);
  cr_set_cfg_var("py", py);
  cr_set_cfg_var("py3k", (vpy >= 30 && vpy < 40) ? "1" : "0");
  cr_set_cfg_var("py2k", (vpy >= 20 && vpy < 30) ? "1" : "0");
  cr_set_cfg_var("py26", (vpy == 26) ? "1" : "0");
  cr_set_cfg_var("py27", (vpy == 27) ? "1" : "0");
  cr_set_cfg_var("py33", (vpy == 33) ? "1" : "0");
  cr_set_cfg_var("py34", (vpy == 34) ? "1" : "0");
  cr_set_cfg_var("py35", (vpy == 35) ? "1" : "0");
  cr_set_cfg_var("py36", (vpy == 36) ? "1" : "0");
  if (!numpy || *numpy == 0)
  {
    vnp = cr_get_cbc_str("numpy", "1.16");
  }
  cr_set_cfg_var("nunpy", numpy);
  cr_set_cfg_var("np", numpy);
  cr_set_cfg_var("pl", "5.26");
  const char *lua = "5"; // see cb in variants.py
  cr_set_cfg_var("lua", "");
  cr_set_cfg_var("luajit", lua[0] == '2' ? "1" : "0");
  // update machines
  // update feature_list
  // adding operating system environment variables ...
  // Add environment based variables
  std::string platform = cr_get_cfg_str("platform", "win");
  if (platform == "win")
    cr_set_cfg_var("PYTHON", "python.exe");
  else
    cr_set_cfg_var("PYTHON", "${PYTHON}");
  std::string v = cr_get_cfg_str("target_platform", "");
  if (v == "" ) cr_set_cfg_var("target_platform", (platform + "-64").c_str());
  v = cr_get_cfg_str("build_platform", "");
  if (v == "" ) cr_set_cfg_var("build_platform", (platform + "-64").c_str());
}
