#include "unistd.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <jinja2cpp/template.h>
#include <jinja2cpp/user_callable.h>
#include <jinja2cpp/generic_list_iterator.h>
#include "prepro.h"
#include "yaml.h"
#include <vector>
#include <filesystem>

#if defined(__APPLE__)
# include <mach-o/dyld.h>
#endif

#if defined(__linux__)
# include <linux/limits.h>
#endif


// externals from Jinja2 debugging
namespace jinja2 {
  extern bool do_expr_debug;
  extern bool do_stmt_debug;
}

// forwarders
static std::string get_feedstockname(const char *fname);
static std::string make_filename(const std::string &arch, const std::string &py2, const std::string &feedname, const char *fname);

typedef std::vector<std::string> vstr_t;

typedef struct archs_t {
  const char *arch;
} archs_t;

std::string prog_name = "crender";
std::filesystem::path data_path;

vstr_t gbl_archs;

vstr_t gbl_cbcs;
const std::string default_odir = "./crender-out";
std::string gbl_odir = default_odir;
vstr_t gbl_ifiles;
vstr_t gbl_python;
bool do_cbc_out = false;
bool ignore_skip = false;
bool no_output_file = false;
int  output_info_files = 0;
bool show_eachprocessed_filename = false;
std::string cur_fname;

archs_t known_archs[] = {
  { "win-32" },
  { "win-64" },
  { "linux-32" },
  { "linux-64" },
  { "linux-ppcle64" },
  { "linux-ppc64" },
  { "linux-aarch64" },
  { "linux-s390x" },
  { "osx-64" },
  { NULL }
};


namespace {

std::filesystem::path get_executable_path() {

  char dest[PATH_MAX];
  memset(dest,0,sizeof(dest)); // readlink does not null terminate!

#if defined(__linux__)
  if (readlink("/proc/self/exe", dest, PATH_MAX) == -1) {
    perror("readlink");
  }

#elif defined(__APPLE__)
  uint32_t size = sizeof(dest);
  if (_NSGetExecutablePath(dest, &size) != 0) {
    perror("_NSGetExecutablePath");
  }

#else
# error "Need to implement for win, etc"
#endif

  std::error_code sec;
  auto rv = std::filesystem::absolute(dest,sec); // Make absolute might not be necessary?
  // Test for a symlink and follow if so.
  if(std::filesystem::is_symlink(rv,sec)) {
    rv = std::filesystem::read_symlink(rv,sec);
  }
  // Report any error
  if(sec) {
    std::cerr << "get_executable_path() error: " << sec << std::endl;
  }
  return rv;
}

void set_prog_vars() {

  const auto exe_path = get_executable_path();
  if(exe_path.empty()) {
    std::cerr << "Empty executable path" << std::endl;
    exit(-1);
  }

  // set filename
  prog_name = exe_path.filename().string();

  // Find and set data_path according to executable location.
  std::error_code sec;
  for(auto i = exe_path.parent_path(); !i.empty(); i = i.parent_path()) {
    auto test_path = i/"data";
    if(std::filesystem::exists(test_path,sec)) {
      data_path = test_path;
      return;
    }
  }

  std::cerr << "Failed to find data path given executable path of " << exe_path << std::endl;
  exit(-1);
}

} // anonymous namespace


static bool is_directory(const char *fname)
{
  struct stat s;
  int err = stat(fname, &s);
  return err != -1 && S_ISDIR(s.st_mode);
}

static void show_usage(const char *fmt,...)
{
  va_list argp;
  va_start (argp, fmt);
  if (fmt != NULL)
  {
    char s[1024];
    vsprintf(s, fmt, argp);
    std::cerr << "*** " << reinterpret_cast<const char *>(&s[0]) << " ***" << std::endl;
  }

  std::cerr << "Usage of " << prog_name << ":" << std::endl
            << "  Version " CRENDER_VERSION << "  (c) 2021" << std::endl;
  std::cerr << "  " << prog_name << " [options] meta.yaml-files" << std::endl << std::endl;
  std::cerr << "  Options:" << std::endl
    << "    -a <arch>    : Add an addition architecture to render" << std::endl
    << "                   If not specified 'linux-64' is used as default" << std::endl
    << "    -c           : Enable output of cbc yaml" << std::endl
    << "    -h           : Show this help" << std::endl
    << "    -m <cbc-file>: Add an additional conda_build_config.yaml file" << std::endl
    << "    -o <dir>     : Specify output directory of rendered file"  << std::endl
    << "                   If '-' is provided output will be done on stdout" << std::endl
    << "                   If not specified '" << default_odir << "' is used as default" << std::endl
    << "    -p <python>  : Specify to be rendered python version" << std::endl
    << "                   If not specified '3.9' is used as default" << std::endl
    << "    -s           : Toggle check of skip in meta.yaml (by default enabled)" << std::endl
    << "    -S           : Toggle check for output of meta.yaml/cbc.yaml (by default enabled)" << std::endl
    << "    -i           : Output information yaml files" << std::endl
    << "    -I           : Like option '-i' but tries to append to existing information yaml files" << std::endl
    << "    -f           : Show each processed file as line" << std::endl
    << "    -v           : Show version information" << std::endl
    << "    -V           : Enable verbose printing" << std::endl;
  ;
  std::cerr << "  Valid arch:";
  for (size_t i = 0; known_archs[i].arch != NULL; i++)
    std::cerr << " " << known_archs[i].arch;
  std::cerr << std::endl;

}

static bool is_in_vstr(const vstr_t &v, const char *str)
{
  for (size_t i = 0; i < v.size(); i++)
    if (v[i] == str) return true;
  return false;
}

static bool add_python(const char *arg)
{
  if ((arg[0] != '2' && arg[0] != '3') || arg[1] != '.' || (arg[2] < '0' && arg[2] > '9'))
  {
    std::cerr << "python version " << arg << "is invalid" << std::endl;
    return false;
  }
  if ( is_in_vstr(gbl_python, arg) )
    return true;
  gbl_python.push_back(arg);
  return true;
}

static bool add_cbc(const char *h, const char *arg)
{
  if ( access(arg, 0) ) { show_usage("file ,%s' does not exist", arg); return false; }
  if ( is_in_vstr(gbl_cbcs, arg) )
  {
    std::cerr << "file ," << h << "' specified mutliple times" << std::endl;
    return true;
  }
  std::cerr << "add " << arg << std::endl;
  gbl_cbcs.push_back(arg);
  return true;
}

static bool add_arch(const char *h, const char *arg)
{
  for (size_t i = 0; known_archs[i].arch != NULL; i++)
  {
    if ( !strcmp(arg, known_archs[i].arch) )
    {
      if ( !is_in_vstr(gbl_archs, arg) )
        gbl_archs.push_back(std::string(arg));
      return true;
    }
  }
  show_usage("illegal architecture '%s' specified by option '%s'", arg, h);
  return false;
}

static bool parse_args(int argc, char **argv)
{
  if ( argc < 1 )
  {
    show_usage(NULL);
    return false;
  }
  bool show_help = false;
  bool status = false;
  bool seen_error = false;

  while (argc-- > 0 )
  {
    char *h = *argv++;
    if (*h != '-' ) {
      if ( access(h, 0) ) { seen_error = true; show_usage("input file ,%s' does not exist", h); }
      if ( !is_in_vstr(gbl_ifiles, h) )
        gbl_ifiles.push_back(std::string(h));
      status = true;
      continue;
    }
    switch (h[1]) {
      default: show_usage("illegal option '%s'", h); seen_error = true; break;
      case 'h': show_help = true; break;
      case 'm':
                if (! argc ) { show_usage("missing argument for option '-m'"); seen_error = true; break; }
                seen_error &= add_cbc(h, *argv++); --argc;
                break;
      case 'a':
                if (! argc ) { show_usage("missing argument for option '%s'", h); seen_error = true; break; }
                seen_error &= add_arch(h, *argv++); --argc;
                break;
      case 'c':
                do_cbc_out = true;
                break;
      case 'f':
                show_eachprocessed_filename = true;
                break;
      case 'o':
                {
                  if (! argc ) {
                    show_usage("missing argument for option '%s'", h);
                    seen_error = true;
                    break;
                  }
                  if ( gbl_odir != default_odir )
                    std::cerr << "  *** warning: overriding already specified output directory ***" << std::endl;
                  gbl_odir = *argv;
                  ++argv; --argc;
                  break;
                }
      case 'p':
                if (! argc ) { show_usage("missing argument for option '%s'", h); seen_error = true; break; }
                seen_error &= add_python(*argv++); --argc;
                break;
      case 's':
                ignore_skip ^= true; break;
      case 'S':
                no_output_file ^= true; break;
      case 'V':
                jinja2::do_expr_debug = true; break;
      case 'v':
                fprintf(stderr, "version: %s\n", CRENDER_VERSION); break;
      case 'W':
                jinja2::do_stmt_debug = true;
                break;
      case 'i':
                output_info_files|=1; break;
      case 'I':
                output_info_files|=3; break;
    }
  }
  if ( show_help && !seen_error )
    show_usage(NULL);
  else if ( !status && !seen_error)
    show_usage("no input files specified");
  return status && !seen_error;
}

static std::string get_file_at_base_of_file(const char *fname, const char *nfname)
{
  std::string r = "";
  const char *h = strrchr(fname, '/');
  if ( h != NULL )
  {
    while(fname < h) r += *fname++;
    r += "/";
  }
  r += nfname;
  if ( access(r.c_str(), 0) )
  {
    // std::cerr << "File " << r << " not found" << std::endl;
    return "";
  }
  return r;
}

// Entry point
int main(int argc, char **argv)
{
  // set initial prog_name and prog_path variables
  set_prog_vars();

  // scan arguments
  if (! parse_args(argc-1, &argv[1]) )
    return 0;

  // ensure output file exists
  std::error_code sec;
  std::filesystem::create_directories(gbl_odir,sec);
  if(sec) {
    std::cerr << "failure in finding/creating output directory: " << gbl_odir << std::endl;
    return -1;
  }

  // set python 3.9 as default, if no python was specified
  if ( gbl_python.size() == 0)
    add_python("3.9");
  // set linux-64 as default architecture, if none was specified
  if ( gbl_archs.size() == 0)
    add_arch("","linux-64");

  // read in statistical stuff, if -I was specified. So iterative extension will work
  if ( (output_info_files & 2) != 0)
      info_outputfiles_read(make_filename("", "", "", "info_"));

  // scan over all python version specified ...
  for (size_t ip = 0; ip < gbl_python.size(); ip++)
  {
    const char *py1 = gbl_python[ip].c_str();
    std::cerr << "  output for python " << py1 << std::endl;
    // created none dotted python version (eg 3.9 -> 39)
    std::string py2 = "";
    {
      const char *h = py1;
      while (*h != 0) { if (*h != '.') py2+=*h++; else h++; }
    }

    for (size_t i = 0; i < gbl_ifiles.size(); i++ )
    {
      std::string fname_o = gbl_ifiles[i];
      std::string cbc_recipe = "";
      const char *fname = fname_o.c_str();

      cur_fname = fname;
      if (show_eachprocessed_filename)
        std::cerr << " Process file " << fname << std::endl;

      // extend file name, if required, so that it points to first meta.yaml
      if ( is_directory(fname) )
      {
        if (fname[strlen(fname)-1] != '/')
          fname_o += "/";
        std::string me;
        me = fname_o + "meta.yaml";
        if ( !access(me.c_str(), 0) )
        {
          fname_o = me;
        }
        else
        {
          // check if we see a conda_build_config.yaml file in the subdirectory
          cbc_recipe = get_file_at_base_of_file(fname_o.c_str(), "conda_build_config.yaml");
          me = fname_o + "recipe/";
          if (is_directory(me.c_str()))
          {
            me += "meta.yaml";
            if ( access(me.c_str(), 0) )
            {
              msg_error("no meta.yaml file found!\n");
              continue;
            }
            fname_o = me;
          }
        }
        fname = fname_o.c_str();
      }
      cur_fname = fname;
      vstr_t cbcs;
      std::string root_cbc = get_file_at_base_of_file("", "conda_build_config.yaml");
      if (root_cbc != "" && !is_in_vstr(gbl_cbcs, root_cbc.c_str()) && !is_in_vstr(cbcs, root_cbc.c_str()))
        cbcs.push_back(root_cbc);
      if (cbc_recipe != "" && !is_in_vstr(gbl_cbcs, cbc_recipe.c_str()) && !is_in_vstr(cbcs, cbc_recipe.c_str()))
        cbcs.push_back(cbc_recipe);
      // add cbc file in same directory as the meta.yaml file, if present
      root_cbc = get_file_at_base_of_file(fname, "conda_build_config.yaml");
      if (root_cbc != "" && !is_in_vstr(gbl_cbcs, root_cbc.c_str()) && !is_in_vstr(cbcs, root_cbc.c_str()))
        cbcs.push_back(root_cbc);

      cbcs.insert(cbcs.end(), gbl_cbcs.begin(), gbl_cbcs.end());
      for (size_t ia = 0; ia < gbl_archs.size(); ia++ )
      {
        // std::cerr << "  for arch " << gbl_archs[ia] << " ..." << std::endl;
        // we read default settings for architecture in front
        std::filesystem::path arch_file = std::string("cbc_") + gbl_archs[ia] + ".yaml";
        std::string arch_cfg = (data_path / arch_file).string();
        if ( !cr_read_config(arch_cfg.c_str(), true, "config") )
        {
          msg_error("failed to read configuration ,%s'\n", arch_cfg.c_str());
          break;
        }
        for (size_t ic = 0; ic < cbcs.size(); ic++ )
        {
          // std::cerr << "  cbc " << cbcs[ic] << " ..." << std::endl;
          if (!cr_read_config(cbcs[ic].c_str(), false) )
          {
            msg_error("failed to read ,%s'", cbcs[ic].c_str());
            break;
          }
        }
        // set py, numpy ...
        cr_set_cfg_preset(py2.c_str(), "");
        // set python variable
        yaml_set_cbc_python_as(gbl_python[ip].c_str());
        if (!cr_read_meta(fname) )
        {
          msg_error("failed to read input file\n");
          break;
        }
        const YAML::Node &th = cr_get_config();
        std::string feedname = get_feedstockname(fname);
        std::string oput = make_filename(gbl_archs[ia], py2, feedname, "meta.yaml");

        bool output_this_feedstock = !no_output_file && (ignore_skip || !cr_has_skip(th["package"]));

        if (output_this_feedstock && !cr_output_yaml_to_file(th["package"], oput.c_str()) )
        {
          msg_error("failed output to file ,%s'\n", oput.c_str());
          break;
        }
        oput = make_filename(gbl_archs[ia], py2, feedname, "conda-build-config.yaml");
        if (output_this_feedstock && do_cbc_out == true && !cr_output_yaml_to_file(th["cbc"], oput.c_str()) )
        {
          msg_error("failed output to file ,%s'\n", oput.c_str());
          break;
        }
      }
    }
  }
  // dump statistical stuff
  if ( output_info_files != 0)
      info_outputfiles(make_filename("", "", "", "info_"));
  return 0;
}

static std::string get_feedstockname(const char *fname)
{
  std::string r;
  const char *fn = strstr(fname, "-feedstock");
  if ( fn != NULL )
  {
    while (fn > fname && (*fn != '/' && *fn != '\\')) fn--;
    if (*fn == '/' || *fn == '\\') fn++;
    while (*fn != 0 && *fn!='/' && *fn != '\\')
     r+=*fn++;
  }
  else
    r = "unknown";
  const char *h = r.c_str();
  fn = strstr(h, "-feedstock");
  if (fn != NULL && h != fn)
    r.erase((fn-h));
  return r;
}

// create file name made out of [<arch>-][-py<python>][-<feedstock-name>](<fname> | unknown.yaml)
static std::string make_filename(const std::string &arch, const std::string &py2, const std::string &feedname, const char *fname)
{
  std::string oput = gbl_odir;

  // output to stdout?
  if ( oput == "-" )
    return oput;

  if (oput[oput.length()-1] != '/' && oput[oput.length()-1] != '\\')
      oput += "/";
  if (!arch.empty())
    oput += arch + "-";
  if (!py2.empty())
    oput += "py" + py2 + "-";
  if ( !feedname.empty() )
  {
      oput += feedname;
      oput += "-";
  }
  if ( !fname || *fname == 0)
    fname = "unnamed.yaml";
  oput += fname;

  return oput;
}

static void msg_output(const char *categ, const char *fmt, va_list argp)
{
  static std::string ofname;
  if ( !show_eachprocessed_filename && ofname != cur_fname )
  {
    std::cerr << "In file " << cur_fname << ":" << std::endl;
    ofname = cur_fname;
  }
  fprintf(stderr,"%s: ", categ);
  vfprintf(stderr, fmt, argp);
}


extern "C"
{
  void msg_error(const char *fmt, ...)
  {
    va_list argp;
    va_start(argp, fmt);
    msg_output("ERROR", fmt, argp);
    va_end(argp);
  }

  void msg_warn(const char *fmt, ...)
  {
    va_list argp;
    va_start(argp, fmt);
    msg_output("WARN", fmt, argp);
    va_end(argp);
  }
};
