#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#ifndef add_info_note
#include "yaml.h"
#include "prepro.h"
#endif
#include "version.h"

#define VCK_EQ 0
#define VCK_NEQ 1
#define VCK_G 3
#define VCK_L 4
#define VCK_GE 5
#define VCK_LE 6
#define VCK_ANY 7
#define VCK_ANYZ 8
#define VCK_EQZ 9
#define VCK_NEQZ 10
#define VCK_ERROR -1

static int read_pkgver_spaces(const char *&h)
{
    int had_space = 0;
    while (*h == ' ') { had_space++; h++; }
    return had_space;
}

std::string read_pgk_name(const char *&h)
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
    bool is_python = name == "pthon";

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
    if (!ver.empty() && is_python)
      add_info_note(pkg, pkver, (x + ": please check if 'python' needs a version specifier").c_str());
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

int get_cmp_kind(const char *cmp)
{
  if (*cmp == 0) return VCK_EQ;
  if (*cmp == '=')
  {
    if (cmp[1] == '=') return VCK_EQZ;
    return VCK_EQ;
  }
  if (*cmp == '!')
  {
    if (cmp[1] == '=') return VCK_NEQZ;
    return VCK_NEQ;
  }
  if (*cmp == '~')
  {
    if (cmp[1] == '=') return VCK_ANYZ;
    return VCK_ANY;
  }
  if (*cmp == '>')
  {
    if (cmp[1] == '=') return VCK_GE;
    return VCK_G;
  }
  if (*cmp == '<')
  {
    if (cmp[1] == '=') return VCK_LE;
    return VCK_L;
  }

  return VCK_ERROR;
}

// a version is made of:
// [<name|digit|star>('.' <name|digit|star>) ]
typedef std::vector<std::string> vec_ver_t;

static bool split_version(vec_ver_t &v, const char *h)
{
  if (*h == 0 || *h == ' ')
    return false;
  while(1) {
    std::string i;
    while(*h != 0 && *h != '.' && *h != ' ')
      i += *h++;
    if (*h == ' ') while(*h != 0) i += *h++;
    if (i.empty())
      i = "0";
    v.push_back(i);
    if (*h == 0)
      break;
    if (*h != '.')
      return false;
    ++h;
  }
  return true;
}

static bool is_num(const std::string &x)
{
  const char *h = x.c_str();
  while(*h >= '0' && *h <= '9') h++;
  if (*h == 'a' && (h[1] >= '0' && h[1] <= '9')) {
    ++h;
    while(*h >= '0' && *h <= '9') h++;
  }
  return *h == 0;
}

static int cmp_eq_version(const char *s, const char *r, bool extend_zero)
{
  vec_ver_t left, right;
  bool lok = split_version(left, s);
  bool rok = split_version(right, r);
  size_t i = 0;
  while (i < left.size() && i < right.size())
  {
    if (left[i] != right[i])
    {
      if (right[i] == "*")
      {
        if ((right.size()-i) == 1)
          return 0;
        i++;
        continue;
      }
      if (is_num(left[i]) && is_num(right[i]))
      {
        int lv = atoi(left[i].c_str());
        int rv = atoi(right[i].c_str());
        return lv - rv;
      }
      return strcmp(left[i].c_str(), right[i].c_str());
    }
    ++i;
  }
  if (i == left.size() && extend_zero)
  {
    while(i < right.size())
    {
      if (right[i] == "*")
      {
        if ((right.size()-i) == 1)
          return 0;
        i++;
        continue;
      }
      if (is_num(right[i]))
      {
        int rv = atoi(right[i].c_str());
        if ( rv != 0)
          return 0 - rv;
      }
      else
        return -1;
      ++i;
    }
  }
  return 0;
}

static int cmp_eq_version2(const char *s, const char *r, bool extend_zero)
{
  bool matched_first = false;
  if (*r == 0 || *r == '*' || !strcmp(s, r))  return 0;
  while (*s != 0 && *r != 0)
  {
    if (*r == '*')  return 0;
    if (*s != *r) break;
    matched_first = true;
    ++s, ++r;
  }
  if (*r == '.' && (r[1] == 0 || r[1] == '*')) ++r;
  if (*s == '.' && s[1] == 0) ++s;
  if ((*s == 0 && *r == 0) || *r == '*')
    return 0;
  if (!matched_first)
    return (*s - *r);
  // 3.9.2 is equal to 3.9 
  if (*r == 0 && (*s == 0 || *s == '.' || s[-1] == '.'))
    return 0;
  // version 3.9 is equal to 3.9.2
  if (*s == 0 && (*r == '.' || *r == 0 || *r == '*') && !extend_zero)
    return 0;
  if (*s != 0 && extend_zero)
  {
    while (*s == '.' || *s == '0')
      ++s;
    if (*s == 0)
      return 0;
  }
  return *s - *r;
}

static bool match_version(const char *cmp, const char *l, const char *r)
{
  int ck = get_cmp_kind(cmp);
  // on error we assume it matches
  if (ck == VCK_ERROR || ck == VCK_ANY || ck == VCK_ANYZ)
    return true;
  switch (ck)
  {
  case VCK_EQ: return cmp_eq_version(l, r, false) == 0; break;
  case VCK_EQZ: return cmp_eq_version(l, r, true) == 0; break;
  case VCK_NEQ: return cmp_eq_version(l, r, false) != 0; break;
  case VCK_NEQZ: return cmp_eq_version(l, r, true) != 0; break;
  case VCK_GE: return cmp_eq_version(l, r, true) >= 0; break;
  case VCK_G: return cmp_eq_version(l, r, true) > 0; break;
  case VCK_LE: return cmp_eq_version(l, r, true) <= 0; break;
  case VCK_L: return cmp_eq_version(l, r, true) < 0; break;
  }
  return false;
}

static bool handle_one_version_fil(const char *ver, const char *&h)
{
  std::string cmp = read_version_cmp(h);
  std::string v = read_version(h);
  return match_version(cmp.c_str(), ver, v.c_str());
}

static bool handle_and_version_fil(const char *ver, const char *&h)
{
  bool f = true;
  while (1)
  {
    f &= handle_one_version_fil(ver, h);
    if (*h != ',') break;
    ++h;
  }
  return f;
}

bool does_version_fit(const char *ver, const char *verspec)
{
  if (!ver || *ver == 0 || (*ver == '*' & *ver == 0))
    return true;
  bool f = false;
  while (1)
  {
    f |= handle_and_version_fil(ver, verspec);
    if (*verspec != '|') break;
    ++verspec;
  }
  return f;
}

std::string modify_version_by_one(const std::string &x, bool add)
{
  if (x.empty() || x[0] == '*') return "";
  const char *h = x.c_str();
  const char *ldot = strrchr(h, '.');
  if (!ldot)
  {
    int a = atoi(h) + add ? 1 : -1;
    char s[256];
    sprintf(s, "%d",a);
    return std::string(s);
  }
  std::string r = "";
  while (h < ldot) r+=*h++;
  {
    int a = atoi(h) + add ? 1 : -1;
    char s[256];
    sprintf(s, "%d",a);
    r += s;
  }
  while (*ldot >= '0' && *ldot <='9') ldot++;
  while (*ldot != 0) r+=*ldot++;
  std::string hr;
  h = r.c_str();
  while (*h != 0)
  {
    if (*h == '*') hr+="0";
    else hr+=*h;
    ++h;
  }
  return hr;
}

std::string get_one_version_match(const std::string &verspecs)
{
  const char *h = verspecs.c_str();
  std::string cmp = read_version_cmp(h);
  std::string v = read_version(h);
  int kind = get_cmp_kind(cmp.c_str());
  if (kind == VCK_ANY || kind == VCK_ERROR || kind == VCK_EQ || kind == VCK_EQZ || kind == VCK_LE || kind ==VCK_GE)
    return v;
  return modify_version_by_one(v, kind == VCK_G || kind == VCK_NEQ || kind == VCK_NEQZ);
}
