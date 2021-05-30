#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "yaml.h"
#include "prepro.h"
#include "version.h"

static int read_pkgver_spaces(const char *&h)
{
    int had_space = 0;
    while (*h == ' ') { had_space++; h++; }
    return had_space;
}

static std::string read_pgk_name(const char *&h)
{
  std::string r = "";
  while ((*h >='a' && *h <= 'z')
   || (*h >='A' && *h <= 'Z')
   || (*h >= '0' && *h <= '9')
   || *h == '-' || *h == '_' || *h == '.')
      r += *h++;
  return r;
}

static std::string read_version_cmp(const char *&h)
{
  std::string r = "";
  if (*h == '<' || *h == '>' || *h == '=' || *h == '!') { r += *h++; }
  if (*h == '~' && h[1] == '=') { r+=*h++; }
  if (*h == '=') { r += *h++; }

  return r;
}

static std::string read_version(const char *&h)
{
    std::string r = "";
    while (1) {
      if (*h == '*') r += *h++;
      else if(*h >='0' && *h <= '9' || (*h >= 'a' && *h <= 'z'))
      {
          while((*h >='0' && *h <= '9') || (*h >= 'a' && *h <= 'z') || (*h >= 'A' && *h <= 'Z') || *h == '_')
            r+= *h++;
        if (*h == '*') r+= *h++;
      }
      else
        break;
      if (*h != '.') break;
      r+=*h++;
    }
    return r;
}

static std::string read_version_string(const char *pkg, const char *pkver,const char *&h, const std::string &x)
{
    std::string r = "";
    while (1)
    {
        std::string hc = read_version_cmp(h);
        std::string hv = read_version(h);
        if (hc.empty() && hv.empty())
          break;
        if (!hc.empty() && hv.empty())
        {
            add_info_note(pkg, pkver, (x + ": unexpected version specifier" +hc+h).c_str());
            return r;
        }
        r += hc + hv;
        if (*h != ',' && *h != '|')
          break;
        r += *h++;
    }
    return r;
}

static bool analyze_pkg_version(const char *pkg, const char *pkver, const std::string &x, std::string &name, std::string &ver)
{
    const char *h = x.c_str();

    name = "";
    ver = "";
    read_pkgver_spaces(h);
    if (*h == 0)
    {
        add_info_note(pkg, pkver, (x + ": empty line in package dependency line").c_str());
        return false;
    }

    name = read_pgk_name(h);
    if (name.empty())
    {
        add_info_note(pkg, pkver, (x + ": no package name provided").c_str());
        return false;
    }
    int had_space = read_pkgver_spaces(h);

    if (*h == 0 || *h == '[')
    {
        if (*h != 0)
            add_info_note(pkg, pkver, (x + ": missing '#' before condition?").c_str());
        return true;
    }
    if (had_space == 0)
      add_info_note(pkg, pkver, (x + ": missing space between package and version").c_str());
    // read the version string
    ver = read_version_string(pkg, pkver, h, x);
    bool ret = true;
    if ( had_space > 1 && !ver.empty())
      add_info_note(pkg, pkver, (x + ": too much spaces between package and version").c_str());
    if (*h != 0 && *h != ' ' && *h != '[')
    {
        add_info_note(pkg, pkver, (x + ": unexpected version specifier").c_str());
        ret = false;
    }
    while (*h != 0 && *h != '[') ++h;
    if (*h == '[')
      add_info_note(pkg, pkver, (x + ": missing '#' before condition?").c_str());
    return ret;
}

bool valdidate_pkg_version(const char *pkg, const char *pkver, const std::string &x)
{
    std::string name;
    std::string ver;
    bool ret = analyze_pkg_version(pkg, pkver, x, name, ver);
    if (!ret)
      return false;
    return true;
}
