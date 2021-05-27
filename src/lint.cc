#include <cstring>
#include <iostream>
#include <string>
#include <cstdlib>
#include <jinja2cpp/template.h>
#include <jinja2cpp/user_callable.h>
#include <jinja2cpp/generic_list_iterator.h>
#include "yaml.h"
#include "prepro.h"

// forwarders ...
static bool has_requires_pkg_in(const char *sname, const YAML::Node &pk, const char *dep, bool check_m2);
static bool has_requires(const YAML::Node &pk, const char *dep, bool check_m2 = false);
static YAML::Node loop_seq(YAML::Node n);
static bool has_compiler_dependency(const YAML::Node &nbase, bool is_output, const YAML::Node &pnbase);
static void check_command_script_line(const char *s, const char *pkname, const char *pkver, const char *m);
static bool has_output_sections(const YAML::Node &nbase);

// external variables
extern YAML::Node theConfig;

// different statistical yaml trees
static YAML::Node output1pkg;
static YAML::Node output2pkg;
static YAML::Node output3pkg;
static YAML::Node output4pkg;
static YAML::Node output5pkg;
static YAML::Node output_notes;
static YAML::Node output_urls;

static void add_usedby(const YAML::Node &n, const char *pkname, const char *pkver)
{
  if (!n.IsDefined())
     return;
  size_t sz = n.size();
  for (size_t i = 0; i < sz; i++)
  {
     std::string dep = cr_get_config_str(n[i], 0, "<undef>");
     info_add_used_by_pkg(pkname, pkver, dep.c_str());
     info_add_depends_on_pkg(pkname, pkver, dep.c_str());
  }
}

static void lint_requirements(const YAML::Node &req, const char *pkname, const char *pkver, const YAML::Node &nbase, bool is_output, const YAML::Node &pnbase)
{
   if ( !req.IsDefined())
   {
    if ( !is_output && !has_output_sections(nbase))
      add_info_note(pkname, pkver, "No requirements section present");
    return;
   }
  if (req.size() == 0)
  {
    add_info_note(pkname, pkver, "Empty requirements section in output present");
    return;
  }
  if (!strcmp(pkname, "wheel") || !strcmp(pkname, "setuptools") || !strcmp(pkname, "pip"))
  {
    if (!has_requires(nbase,"python") && (!is_output || !has_requires(pnbase, "python")))
      add_info_note(pkname, pkver, "Missing python package in requirements");
    if (strcmp(pkname, "wheel") != 0)
        if (!has_requires(nbase,"wheel") && (!is_output || !has_requires(pnbase, "wheel")))
            add_info_note(pkname, pkver, "Requirement wheel has not to be present");
    if (strcmp(pkname, "pip") != 0)
        if (!has_requires(nbase,"pip") && (!is_output || !has_requires(pnbase, "pip")))
            add_info_note(pkname, pkver, "Requirement pip has not to be present");
    if (strcmp(pkname, "setuptools") != 0)
        if (!has_requires(nbase,"setuptools") && (!is_output || !has_requires(pnbase, "setuptools")))
            add_info_note(pkname, pkver, "Requirement setuptools has not to be present");
  }
  else if (has_requires(nbase,"pip") || has_requires(nbase,"wheel") || has_requires(nbase,"setuptools"))
  {
    if (!has_requires(nbase, "pip") && (!is_output  || !has_requires(pnbase, "pip")))
      add_info_note(pkname, pkver, "Missing pip package in requirements");
    if (!has_requires(nbase,"python") && (!is_output || !has_requires(pnbase, "python")))
      add_info_note(pkname, pkver, "Missing python package in requirements");
    if (!has_requires(nbase,"wheel") && (!is_output || !has_requires(pnbase, "wheel")))
      add_info_note(pkname, pkver, "Missing wheel package in requirements");
    if (!has_requires(nbase,"setuptools") && (!is_output || !has_requires(pnbase, "setuptools")))
      add_info_note(pkname, pkver, "Missing setuptools package in requirements");
  }
}

static void lint_build(const YAML::Node &build, const char *pkname, const char *pkver, const YAML::Node &nbase, bool is_output, const YAML::Node &pnbase)
{
  if (!build.IsDefined())
  {
    if ( !is_output)
      add_info_note(pkname, pkver, !has_output_sections(pnbase) ? "No build section present" : "Build section should contain at least build-number");
    return;
  }
  if (build.size() == 0)
  {
    add_info_note(pkname, pkver, "Empty build section in output present");
    return;
  }
  if (build["script"].IsDefined())
  {
    if (build["script"].IsSequence())
    {
      size_t sz = build["script"].size();
      if (sz == 0)
        add_info_note(pkname, pkver, "Build sections contains empty script");
      for (size_t i = 0; i <sz; i++)
      {
        std::string script = cr_get_config_str(build["script"], i, "");
        const char *s = script.c_str();
        while (*s > 0 && *s <=0x20)
            ++s;
        if (*s == '"')
        {
            ++s;
            while (*s > 0 && *s <=0x20)
                ++s;
            if (*s == '"') ++s;
        }
        if (*s == '\'')
        {
            ++s;
            while (*s > 0 && *s <=0x20)
                ++s;
            if (*s == '\'') ++s;
        }
        if (*s == 0)
          add_info_note(pkname, pkver, "Build section contains an empty 'script' line");
        else if (s[0] == '-')
          add_info_note(pkname, pkver, "Build section contains 'script' line with implicit python invocation");
        else
          check_command_script_line(s, pkname, pkver, "In 'script' line in Build section ");

      }
    }
    else
    {
      std::string script = cr_get_config_str(build["script"], 0, "");
      const char *s = script.c_str();
      while (*s > 0 && *s <=0x20)
          ++s;
      if (*s == '"')
      {
          ++s;
          while (*s > 0 && *s <=0x20)
              ++s;
          if (*s == '"') ++s;
      }
      if (*s == '\'')
      {
          ++s;
          while (*s > 0 && *s <=0x20)
              ++s;
          if (*s == '\'') ++s;
      }
      if (*s == 0)
        add_info_note(pkname, pkver, "Build section contains empty 'script'");
      else if (s[0] == '-')
        add_info_note(pkname, pkver, "Build section contains 'script' with implicit python invocation");
      else
        check_command_script_line(s, pkname, pkver, "In 'script' in Build section ");
    }
  }
  if (!build["number"].IsDefined() && !is_output)
    add_info_note(pkname, pkver, "Build section without build-number");
  if (build["number"].IsDefined() && is_output)
    add_info_note(pkname, pkver, "Build section in output specifies build-number");
  bool seen_compiler = has_compiler_dependency(nbase, is_output, pnbase);
  if (build["noarch"].IsDefined())
  {
    std::string noarch = cr_get_config_str(build["noarch"], 0, "");
    if (noarch.empty())
      add_info_note(pkname, pkver, is_output ? "Build section in output specifies empty noarch" : "Build section specifies empty noarch");
    else if (noarch != "python" && noarch != "generic")
    {
      std::string msg = "Build section specifies unknown noarch type ,";
      msg += noarch;
      msg += "'";
      if ( is_output)
        msg += " in output";
      add_info_note(pkname, pkver, msg.c_str());
    }
    else if (noarch == "python"
             && !has_requires(nbase, "python", false)
             && (!is_output || !has_requires(pnbase, "python")))
    {
      add_info_note(pkname, pkver,
        !is_output ? "Build section specifies noarch-python but no python package specified"
                   : "Build section specifies noarch-python but no python package in output specified");
    }
    if ( seen_compiler && !noarch.empty())
      add_info_note(pkname, pkver,
        !is_output ? "Build section specifies noarch but compiler package specified"
                   : "Build section specifies noarch but compiler package possibly in output specified");
  }
  else
  {
    if (!seen_compiler)
      add_info_note(pkname, pkver,
        !is_output ? "Build section might be actual noarch"
                   : "Build section might be actual noarch in output specifier");
  }
  if (build["run_exports"].IsDefined())
  {
    if (build["run_exports"].size() == 0)
      add_info_note(pkname, pkver, "In build section empty run_exports specified");
  }
  if (build["ignore_run_exports"].IsDefined())
  {
    if (build["ignore_run_exports"].size() == 0)
      add_info_note(pkname, pkver, "In build section empty ignore_run_exports specified");
  }
  if (build["missing_dso_whitelist"].IsDefined())
  {
    if (build["missing_dso_whitelist"].size() == 0)
      add_info_note(pkname, pkver, "In build section empty missing_dso_whitelist specified");
  }
  if (build["script_env"].IsDefined())
  {
    if (build["script_env"].size() == 0)
      add_info_note(pkname, pkver, "In build section empty script_env specified");
  }
  if (build["string"].IsDefined())
  {
    std::string str = cr_get_config_str(build["string"], 0, "");
    if (str.empty())
      add_info_note(pkname, pkver, "In build section empty string specified");
    else
    {
      std::string msg = "In build section string specified as ,";
      msg += str;
      msg += "'";
      add_info_note(pkname, pkver, msg.c_str());
    }
  }
  if (build["merge_build_host"].IsDefined())
  {
    std::string str = cr_get_config_str(build["merge_build_host"], 0, "False");
    if (str == "True" || str == "true")
      add_info_note(pkname, pkver, "In build section 'merge_build_host' kludge used");
    else
      add_info_note(pkname, pkver, "In build section 'merge_build_host' kludge has to be removed");
  }
}

static std::string cr_get_key_ovr(const YAML::Node &n, bool use_parent, const YAML::Node &pn)
{
  std::string msg = cr_get_config_str(n, 0, "");
  if (msg.empty() && n.IsDefined() && n.IsSequence())
    msg="muliple keys";
  if (msg.empty() && use_parent && pn.IsDefined() && pn.IsSequence())
    msg="muliple keys from parent";
  if (msg.empty() && use_parent && pn.IsDefined())
    msg = cr_get_config_str(pn, 0, "");
  return msg;
}

static void lint_about(const YAML::Node &about, const char *pkname, const char *pkver, const YAML::Node &, bool is_output, const YAML::Node &pn)
{
  const YAML::Node &pabout = (is_output && pn.IsDefined() && pn["about"].IsDefined()) ? pn["about"] : pn;
  if (!about.IsDefined())
  {
    if (!is_output || !pabout.IsDefined())
      add_info_note(pkname, pkver, is_output ? "No about section in output present" : "No about section present");
    return;
  }
  std::string msg;

  msg = cr_get_key_ovr(about["home"], is_output, pabout["home"]);
  if (msg.empty())
    add_info_note(pkname, pkver, "No home specified");
   else
    add_info_url(msg.c_str(), "home", pkname, pkver, "", "");
  msg = cr_get_key_ovr(about["license"], is_output, pabout["license"]);
  if (msg.empty())
    add_info_note(pkname, pkver, "No license specified");

  msg = cr_get_key_ovr(about["license_family"], is_output, pabout["license_family"]);
  if (msg.empty())
    add_info_note(pkname, pkver, "No license family specified");

  msg = cr_get_key_ovr(about["license_file"], is_output, pabout["license_file"]);
  if (msg.empty())
    add_info_note(pkname, pkver, "No license file(s) specified");

  msg = cr_get_key_ovr(about["summary"], is_output, pabout["summary"]);
  if (msg.empty())
    add_info_note(pkname, pkver, "No summary provided");

  msg = cr_get_key_ovr(about["doc_url"], is_output, pabout["doc_url"]);
  if (msg.empty())
    add_info_note(pkname, pkver, "No documentation url provided");
  else
    add_info_url(msg.c_str(), "doc", pkname, pkver, "", "");

  msg = cr_get_key_ovr(about["dev_url"], is_output, pabout["dev_url"]);
  if (msg.empty())
    add_info_note(pkname, pkver, "No development url provided");
  else
    add_info_url(msg.c_str(), "dev", pkname, pkver, "", "");
}

static void lint_test(const YAML::Node &test, const char *pkname, const char *pkver, const YAML::Node &nbase, bool is_output, const YAML::Node &pnbase)
{
  const YAML::Node &ptest = (is_output && pnbase.IsDefined() && pnbase["test"].IsDefined()) ? pnbase["test"] : test;
  if (!test.IsDefined())
  {
    if ((!is_output && has_output_sections(nbase))|| !ptest.IsDefined())
      add_info_note(pkname, pkver, is_output ? "No test section in output present" : "No test section present");
    return;
  }
  // requires
  bool has_python = false;
  bool has_python_req = has_python || has_requires(nbase, "python") || (is_output && has_requires(pnbase, "python"));
  bool has_compiler = has_compiler_dependency(nbase, is_output, pnbase);
  if (ptest["requires"].IsDefined())
  {
    if (ptest["requires"].size() == 0 || !ptest["requires"].IsSequence())
      add_info_note(pkname, pkver, "Test section contains empty 'requires'");
    has_python = has_requires_pkg_in("requires", nbase, "python", false);
  }
  // see imports
  if (ptest["imports"].IsDefined())
  {
      if (!ptest["imports"].IsSequence() || ptest["imports"].size() == 0)
        add_info_note(pkname, pkver, "Test section contains empty 'imports'");
      if (!has_python_req)
        add_info_note(pkname, pkver, "Test section contains 'imports' test, but package has not 'python' package in requirements");
  }
  else
  {
    if (has_python_req && !has_compiler)
      add_info_note(pkname, pkver, "Test section contains no 'imports', but package seems to be a python package");
  }
  // see commands
  if (ptest["commands"].IsDefined())
  {
    if (!ptest["commands"].IsSequence() || ptest["commands"].size() == 0)
      add_info_note(pkname, pkver, "Test section contains empty 'commands' table");
    else
    {
        size_t sz = ptest["commands"].size();
        for (size_t i = 0; i < sz; i++)
        {
            std::string c = cr_get_config_str(test["commands"], i, "");
            check_command_script_line(c.c_str(), pkname, pkver, "Line in 'commands' of test section ");
        }
    }
  }
  else
  {
    if (has_compiler)
      add_info_note(pkname, pkver, "Test section contains no 'commands' table, but uses compiler(s)");
    else if (has_python_req)
       add_info_note(pkname, pkver, "Test section contains no 'commands'");
  }
  // downstreams
  if (ptest["downstreams"].IsDefined())
  {
    if (!ptest["downstreams"].IsSequence() || ptest["downstreams"].size() == 0)
      add_info_note(pkname, pkver, "Test section contains empty 'downstreams' table");
  }
}

// check source section:
// source:
//   list or single item of url/git_url/path and patches
static void lint_source(const YAML::Node &source, const char *pkname, const char *pkver, const YAML::Node &nbase)
{
  if (!source.IsDefined())
  {
    add_info_note(pkname, pkver, "No source section present");
    return;
  }
  if (source.size() == 0)
  {
    add_info_note(pkname, pkver, "Empty source section present");
    return;
  }
  if (source.IsSequence())
  {
    size_t sz = source.size();
    for (size_t i = 0; i < sz; i++)
      lint_source(source[i], pkname, pkver, nbase);
    return;
  }
  int s = source["url"].IsDefined() ? 1 : 0;
  s += source["git_url"].IsDefined() ? 1 : 0;
  s += source["path"].IsDefined() ? 1 : 0;
  if (s == 0)
    add_info_note(pkname, pkver, "No input source specified");
  else if ( s != 1 )
    add_info_note(pkname, pkver, "Multiple input sources specified");
  if (source["git_url"].IsDefined())
  {
    if (!has_requires_pkg_in("build", nbase, "git", true))
      add_info_note(pkname, pkver, "Missing (m2-)git in 'build' requirements");
    add_info_url(cr_get_config_str(source["git_url"], 0, "").c_str(), "git", pkname, pkver, "", "");
  }
  if (source["url"].IsDefined())
  {
    int v = source["sha256"].IsDefined() ? 1 : 0;
    v += source["sha1"].IsDefined() ? 1 : 0;
    v += source["md5"].IsDefined() ? 1 : 0;
    if (v == 0)
      add_info_note(pkname, pkver, "Missing sha/md5 for source url");
    else if ( v != 1)
      add_info_note(pkname, pkver, "Multiple sha/md5 specified for one source url");
    add_info_url(cr_get_config_str(source["url"], 0, "").c_str(), "file", pkname, pkver, "", "");
    // TODO: output for each crc kind
  }

  if (source["patches"].IsDefined())
  {
    if (source["patches"].size() == 0)
      add_info_note(pkname, pkver, "Has an empty patches in source section");
    else if (!has_requires_pkg_in("build", nbase, "patch", true))
      add_info_note(pkname, pkver, "Missing (m2-)patch build requirement");
  }
}

// We scan the following places for version:
// requirements:
// and 'test: requires:'
void check_for_package_version(const std::string &fname)
{
  const YAML::Node &n = theConfig["package"];
  if (!n.IsDefined())
    return;
  const YAML::Node pkg = n["package"];
  std::string pkgnamestr = fname.c_str();
  std::string pkgversion = "<unspecified>";
  if (pkg.IsDefined())
  {
    pkgnamestr = cr_get_config_str(pkg["name"], 0, fname.c_str());
    pkgversion = cr_get_config_str(pkg["version"], 0, "<unspecified>");
    YAML::Node nn = pkg["name"];
    YAML::Node nnver = pkg["version"];
    if (nn.IsDefined() && nnver.IsDefined())
    {
      if (theConfig["cbc"][nn].IsDefined())
        theConfig["cbc"].remove(nn);
      theConfig["cbc"][nn][0] = nnver;
    }
  }

  // check source section
  lint_source(n["source"], pkgnamestr.c_str(), pkgversion.c_str(), n);
  // check build section
  lint_build(n["build"], pkgnamestr.c_str(), pkgversion.c_str(), n, false, n);
  lint_requirements(n["requirements"], pkgnamestr.c_str(), pkgversion.c_str(), n, false, n);
  lint_test(n["test"], pkgnamestr.c_str(), pkgversion.c_str(), n, false, n);

  if (n["outputs"].IsDefined() && n["outputs"].size() > 0)
  {
    size_t sz = n["outputs"].size();
    for (size_t i = 0; i < sz; i++)
    {
      const YAML::Node no = n["outputs"][i];
      lint_build(no["build"], pkgnamestr.c_str(), pkgversion.c_str(), no, true, n);
      lint_requirements(no["requirements"], pkgnamestr.c_str(), pkgversion.c_str(), no, true, n);
      lint_test(no["test"], pkgnamestr.c_str(), pkgversion.c_str(), no, true, n);
      lint_about(no["about"], pkgnamestr.c_str(), pkgversion.c_str(), no, true, n);
    }
  }
  // check about section
  lint_about(n["about"], pkgnamestr.c_str(), pkgversion.c_str(), n, false, n);

  info_add_pkg_version(pkgnamestr.c_str(), pkgversion.c_str());
  info_add_output_2_pkg(pkgnamestr.c_str(), pkgnamestr.c_str());
  info_add_pkg_2_output(pkgnamestr.c_str(), pkgversion.c_str(), pkgnamestr.c_str());
  const YAML::Node &nr = n["requirements"];
  if (nr.IsDefined())
  {
    YAML::Node nn;
    nn = loop_seq(theConfig["package"]["requirements"]["build"]);
    if (nn.IsDefined() && nn.size() > 0)
    {
      add_usedby(nn, pkgnamestr.c_str(), pkgversion.c_str());
      theConfig["package"]["requirements"].remove("build");
      theConfig["package"]["requirements"]["build"] = nn;
    }
    nn = loop_seq(theConfig["package"]["requirements"]["host"]);
    if (nn.IsDefined() && nn.size() > 0)
    {
      add_usedby(nn, pkgnamestr.c_str(), pkgversion.c_str());
      theConfig["package"]["requirements"].remove("host");
      theConfig["package"]["requirements"]["host"] = nn;
    }
    nn = loop_seq(theConfig["package"]["requirements"]["run"]);
    if (nn.IsDefined() && nn.size() > 0)
    {
      add_usedby(nn, pkgnamestr.c_str(), pkgversion.c_str());
      theConfig["package"]["requirements"].remove("run");
      theConfig["package"]["requirements"]["run"] = nn;
    }
  }
  const YAML::Node &tn = n["test"];
  if (tn.IsDefined())
  {
    YAML::Node nn;
    nn = loop_seq(theConfig["package"]["test"]["requires"]);
    if (nn.IsDefined() && nn.size() > 0)
    {
      add_usedby(nn, pkgnamestr.c_str(), pkgversion.c_str());
      theConfig["package"]["test"].remove("requires");
      theConfig["package"]["test"]["requires"] = nn;
    }
  }
  const YAML::Node &opts = n["outputs"];
  if (opts.IsDefined())
  {
    size_t sn = opts.size();
    for (size_t i = 0; i < sn; i++)
    {
      std::string nstr = cr_get_config_str(opts[i]["name"], 0, "<unspecified>");
      info_add_output_2_pkg(nstr.c_str(), pkgnamestr.c_str());
      info_add_pkg_2_output(pkgnamestr.c_str(), pkgversion.c_str(), nstr.c_str());
      const YAML::Node &r1 = opts[i]["requirements"];
      if (r1.IsDefined())
      {
        YAML::Node nn;
        if (r1["build"].IsDefined())
        {
          nn = loop_seq(theConfig["package"]["outputs"][i]["requirements"]["build"]);
          if (nn.IsDefined() && nn.size() > 0)
          {
            add_usedby(nn, pkgnamestr.c_str(), pkgversion.c_str());
            theConfig["package"]["outputs"][i]["requirements"].remove("build");
            theConfig["package"]["outputs"][i]["requirements"]["build"] = nn;
          }
        }
        if (r1["host"].IsDefined())
        {
          nn = loop_seq(theConfig["package"]["outputs"][i]["requirements"]["host"]);
          if (nn.IsDefined() && nn.size() > 0)
          {
            add_usedby(nn, pkgnamestr.c_str(), pkgversion.c_str());
            theConfig["package"]["outputs"][i]["requirements"].remove("host");
            theConfig["package"]["outputs"][i]["requirements"]["host"] = nn;
          }
        }
        if (r1["run"].IsDefined())
        {
          nn = loop_seq(theConfig["package"]["outputs"][i]["requirements"]["run"]);
          if (nn.IsDefined() && nn.size() > 0)
          {
            add_usedby(nn, pkgnamestr.c_str(), pkgversion.c_str());
            theConfig["package"]["outputs"][i]["requirements"].remove("run");
            theConfig["package"]["outputs"][i]["requirements"]["run"] = nn;
          }
        }
      }
      const YAML::Node &r2 = opts[i]["test"];
      if (r2.IsDefined())
      {
        YAML::Node nn;
        nn = loop_seq(theConfig["package"]["outputs"][i]["test"]["requires"]);
        if (nn.IsDefined() && nn.size() > 0)
        {
          add_usedby(nn, pkgnamestr.c_str(), pkgversion.c_str());
          theConfig["package"]["outputs"][i]["test"].remove("requires");
          theConfig["package"]["outputs"][i]["test"]["requires"] = nn;
        }
      }
    }
  }
}

static bool has_output_sections(const YAML::Node &nbase)
{
  if (!nbase.IsDefined())
    return false;
  return nbase["outputs"].IsDefined();
}

void info_outputfiles(const std::string &fname_prefix)
{
  if (fname_prefix == "-")
  {
    cr_output_yaml_to_file(output1pkg, "-");
    cr_output_yaml_to_file(output2pkg, "-");
    cr_output_yaml_to_file(output3pkg, "-");
    cr_output_yaml_to_file(output4pkg, "-");
    cr_output_yaml_to_file(output5pkg, "-");
    cr_output_yaml_to_file(output_notes, "-");
    cr_output_yaml_to_file(output_urls, "-");
  }
  else
  {
    cr_output_yaml_to_file(output1pkg, (fname_prefix + "o2p.yaml").c_str());
    cr_output_yaml_to_file(output2pkg, (fname_prefix + "pv.yaml").c_str());
    cr_output_yaml_to_file(output3pkg, (fname_prefix + "pv2o.yaml").c_str());
    cr_output_yaml_to_file(output4pkg, (fname_prefix + "p_usedby.yaml").c_str());
    cr_output_yaml_to_file(output5pkg, (fname_prefix + "p_dependson.yaml").c_str());
    cr_output_yaml_to_file(output_notes, (fname_prefix + "notes.yaml").c_str());
    cr_output_yaml_to_file(output_urls, (fname_prefix + "urls.yaml").c_str());
  }
}

// 1. outputs -> to package
void info_add_output_2_pkg(const char *oname, const char *pkgname)
{
  if (!oname || *oname == 0 || pkgname == NULL || *pkgname == 0)
    return;
  const YAML::Node &n = output1pkg[oname];
  size_t nsz = 0;
  if (n.IsDefined())
  {
    nsz = n.size();
    for (size_t i = 0; i < nsz; i++)
    {
      std::string nstr = cr_get_config_str(n, i, "");
      if (nstr == pkgname)
         return; // we are done, element already there
    }
  }
  output1pkg[oname][nsz] = YAML::Node(pkgname);
}

// 2. package -> versions
void info_add_pkg_version(const char *pkgname, const char *version)
{
  if (pkgname == NULL || *pkgname == 0)
    return;
  if (version == NULL || *version == 0)
    version = "*";
  const YAML::Node &n = output2pkg[pkgname];
  size_t nsz = 0;
  if (n.IsDefined())
  {
    nsz = n.size();
    for (size_t i = 0; i < nsz; i++)
    {
      std::string nstr = cr_get_config_str(n, i, "");
      if (nstr == version)
        return;
    }
  }
  output2pkg[pkgname][nsz]=YAML::Node(version);
}

// 3. package ->output
void info_add_pkg_2_output(const char *pkgname, const char *version, const char *oname)
{
  if (!oname || *oname == 0 || pkgname == NULL || *pkgname == 0)
    return;
  if (version == NULL)
    version = "";
  std::string pn = pkgname;
  if (*version != 0) {
      pn += " ";
      pn += version;
  }
  const YAML::Node &n = output3pkg[pn.c_str()];
  size_t nsz = 0;
  if (n.IsDefined())
  {
    nsz = n.size();
    for (size_t i = 0; i < nsz; i++)
    {
      std::string nstr = cr_get_config_str(n, i, "");
      if (nstr == oname)
         return; // we are done, element already there
    }
  }
  output3pkg[pn.c_str()][nsz] = YAML::Node(oname);
}

void info_add_used_by_pkg(const char *pkname, const char *nver, const char *dep)
{
  if (!dep || *dep == 0 || !pkname || *pkname == 0)
    return;
  char *h = (char *) alloca(strlen(dep)+1);
  strcpy(h, dep);
  char *hp = strchr(h, ' ');
  if ( hp) { *hp++ = 0; }
  const char *ver = hp;
  dep = h;
  if (ver == NULL || *ver == 0)
    ver = "*";
  std::string pn = pkname;
  if (nver && *nver != 0)
  {
    pn += " ";
    pn += nver;
  }
  size_t nsz = 0;
  const YAML::Node &n2 = output4pkg[dep][ver];
  if (n2.IsDefined())
  {
    nsz = n2.size();
    for (size_t i = 0; i < nsz; i++)
    {
      std::string nstr = cr_get_config_str(n2, i, "");
      if (nstr == pn)
        return; // we are done, element already there
    }
  }
  if (nsz == 0)
  {
    YAML::Node nn;
    nn[0] = YAML::Node(pn.c_str());
    output4pkg[dep][ver]=nn;
  }
  else
    output4pkg[dep][ver][nsz] = YAML::Node(pn.c_str());
}

static bool has_requires_pkg_in(const char *sname, const YAML::Node &pk, const char *dep, bool check_m2)
{
  if (!pk.IsDefined()
      || !pk["requirements"].IsDefined()
      || (sname != NULL && *sname != 0 && !pk["requirements"][sname].IsDefined()))
    return false;
  const YAML::Node &n =  (sname == NULL || *sname == 0) ? pk["requirements"] : pk["requirements"][sname];
  if (!n.IsSequence())
     return false;
  size_t sz = n.size();
  size_t l = strlen(dep);
  for (size_t j = 0; j < sz; j++)
  {
    std::string msg = cr_get_config_str(n, j, "");
    const char *h = msg.c_str();
    if (check_m2 && h[0] == 'm' && h[1] == '2' && h[2] == '-')
      h+=3;
    size_t i = 0;
    while (dep[i] != 0 && dep[i] == h[i])
      ++i;
    if (dep[i] == 0 && (h[i] == 0 || h[i] == ' '))
      return true;
  }
  return false;
}

static bool has_requires(const YAML::Node &pk, const char *dep, bool check_m2)
{
  if (!pk.IsDefined() || !pk["requirements"].IsDefined())
    return false;
  if (has_requires_pkg_in("build", pk, dep, check_m2))
    return true;
  if (has_requires_pkg_in("host", pk, dep, check_m2))
    return true;
  if (has_requires_pkg_in("run", pk, dep, check_m2))
    return true;
  if (pk["requirements"].IsSequence())
    if (has_requires_pkg_in("", pk, dep, check_m2))
        return true;
    return false;
}

// loop over a given sequence of package names and
// extend them, if required, by their version pinning
// taken from cbc.
static YAML::Node loop_seq(YAML::Node n)
{
  YAML::Node r;
  if (!n.IsDefined() || !n.IsSequence())
    return r;
  size_t nmax = n.size();
  if (nmax == 0)
    return r;
  for (size_t i = 0; i <nmax; i++)
  {
    std::string strr = cr_get_config_str(n, i, "");

    if (strchr(strr.c_str(), ' ') == NULL)
    {
      std::string cbc_val = cr_get_cbc_str(strr.c_str(), "");
      if (!cbc_val.empty())
      {
        strr += " ";
        strr += cbc_val;
      }
    }
    r[i] = YAML::Node(strr);
  }
  return r;
}

// 4. package -> used-urls + sha
void info_add_pkg_2_url(const char *pkgname, const char *version, const char *url, const char *sha);

// 5. package depends on run/host/build/test
void info_add_depends_on_pkg(const char *pkname, const char *nver, const char *dep)
{
  if (!dep || *dep == 0)
    return;
  std::string pn = pkname;
  if (nver && *nver != 0)
  {
    pn += " ";
    pn += nver;
  }
  size_t nsz = 0;
  const YAML::Node &n2 = output5pkg[pn.c_str()];
  if (n2.IsDefined())
  {
    nsz = n2.size();
    for (size_t i = 0; i < nsz; i++)
    {
      std::string nstr = cr_get_config_str(n2, i, "");
      if (nstr == dep)
        return; // we are done, element already there
    }
  }
  output5pkg[pn.c_str()][nsz]=YAML::Node(dep);
}

// package version used by package

void info_outputfiles_read(const std::string &fname_prefix)
{
  // nothing to do as output/input will be done to console
  if (fname_prefix == "-")
     return;
  output1pkg = cr_read_yaml((fname_prefix + "o2p.yaml").c_str());
  output2pkg = cr_read_yaml((fname_prefix + "pv.yaml").c_str());
  output3pkg = cr_read_yaml((fname_prefix + "pv2o.yaml").c_str());
  output4pkg = cr_read_yaml((fname_prefix + "p_usedby.yaml").c_str());
  output5pkg = cr_read_yaml((fname_prefix + "p_dependson.yaml").c_str());
  output_notes = cr_read_yaml((fname_prefix + "notes.yaml").c_str());
  output_urls = cr_read_yaml((fname_prefix + "urls.yaml").c_str());
}

void add_info_url(const char *url, const char *url_kind, const char *pkg, const char *ver, const char *crc_kind, const char *crc)
{
  if (!url || *url == 0 || pkg == NULL || *pkg == 0)
    return;
  if (!url_kind || *url_kind == 0)
    url_kind="<unspecified-kind>";
  if (!ver)
    ver = "";
  if (!crc_kind)
    crc_kind = "";
  if (!crc)
    crc = "<unspecified>";
  std::string pn = pkg;
  if (*ver != 0)
  {
    pn += " ";
    pn += ver;
  }
  const YAML::Node &n = output_urls[url];
  if (!n.IsDefined() || !n["kind"].IsDefined())
  {
    if (!output_urls["urls"].IsDefined())
      output_urls["urls"][0] = YAML::Node(url);
    else
    {
      output_urls["urls"][output_urls["urls"].size()] = YAML::Node(url);
    }
    output_urls[url]["kind"][0] = YAML::Node(url_kind);
  }
  else
  {
    const YAML::Node &nkind = output_urls[url]["kind"];
    size_t i = 0;
    size_t sz = nkind.size();
    for (;i<sz;i++)
    {
      std::string msg = cr_get_config_str(nkind, i, "");
      if (msg == url_kind)
        break;
    }
    if ( i == sz)
      output_urls[url]["kind"][sz] = YAML::Node(url_kind);
  }
  if (*crc_kind != 0)
  {
    const YAML::Node &nc = output_urls[url][crc_kind];
    size_t sz = 0;
    if (nc.IsDefined())
    {
      size_t i = 0;
      sz = nc.size();
      for (; i < sz; i++)
      {
        std::string msg = cr_get_config_str(nc, i, "");
        if (msg == crc)
          break;
      }
      if ( i == sz)
        output_urls[url][crc_kind][sz] = YAML::Node(crc);
    }
    else
      output_urls[url][crc_kind][sz] = YAML::Node(crc);
  }
  {
    const YAML::Node &nc = output_urls[url]["used_by"];
    size_t sz = 0;
    if (nc.IsDefined())
    {
      size_t i = 0;
      sz = nc.size();
      for (; i < sz; i++)
      {
        std::string msg = cr_get_config_str(nc, i, "");
        if (msg == pn)
          break;
      }
      if ( i == sz)
        output_urls[url]["used_by"][sz] = YAML::Node(pn.c_str());
    }
    else
      output_urls[url]["used_by"][sz] = YAML::Node(pn.c_str());
  }
}

void add_info_note(const char *pkg, const char *ver, const char *note)
{
  if (!note || *note == 0)
    return;
  if (!ver || *ver == 0)
    ver = "*";
  YAML::Node n = output_notes[pkg][ver];
  size_t sz = 0;
  if (n.IsDefined())
    sz = n.size();
  for (size_t i = 0; i < sz; i++)
  {
    std::string msg = cr_get_config_str(n, i, "");
    if (msg == note)
      return;
  }
  output_notes[pkg][ver][sz] = YAML::Node(note);
}

static bool has_compiler_dependency(const YAML::Node &nbase, bool is_output, const YAML::Node &pnbase)
{
    static const char *cmp_names[] = { "c", "cxx", "fortran",
        "m2w64_c", "m2w64_cxx", "m2w64_fortran", "rust", "rust-gnu",
        "cuda", "opencl", "go", "haskell", NULL};
    for (size_t i = 0; cmp_names[i] != NULL; i++)
    {
        std::string compiler = expand_compiler_int(cmp_names[i], false);
        if (has_requires(nbase, compiler.c_str()) || (is_output && has_requires(pnbase, compiler.c_str())))
            return true;
    }
    return false;
}

static void check_command_script_line(const char *s, const char *pkname, const char *pkver, const char *m)
{
    std::string msg;
    while (*s >= 0 && *s <= 0x20)
        ++s;
    if (*s == '\'')
    {
        ++s;
        while (*s >= 0 && *s <= 0x20)
            ++s;
        if (*s == '\'')
          ++s;
        while (*s >= 0 && *s <= 0x20)
            ++s;
    }
    if (*s == '\"')
    {
        ++s;
        while (*s >= 0 && *s <= 0x20)
            ++s;
        if (*s == '\"')
          ++s;
        while (*s >= 0 && *s <= 0x20)
            ++s;
    }
    if (*s == 0)
    {
        msg = m;
        msg += "contains empty line";
        add_info_note(pkname, pkver, msg.c_str());

    }
    if (!strcmp(s, "python "))
    {
        msg = m;
        msg += "invokes python without using environment variable PYTHON";
        add_info_note(pkname, pkver, msg.c_str());
    }
    // check for wrong environment variable encodings
    std::string platform = cr_get_cfg_str("platform", "win");
    if (platform == "win")
    {
        if (strstr(s, "${"))
        {
            msg = m;
            msg += "uses unix-style environment variable on windows shell";
            add_info_note(pkname, pkver, msg.c_str());
        }
    } else {
        const char *h = strchr(s, '%');
        if (h != NULL && strchr(h+1, '%') != NULL)
        {
            msg = m;
            msg += "uses windows-style environment variable on unix shell";
            add_info_note(pkname, pkver, msg.c_str());
        }
    }
}