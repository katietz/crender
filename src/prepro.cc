#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "yaml.h"
#include "prepro.h"

// Token IDs definition
#define TK_EQ '='
#define TK_L '<'
#define TK_G '>'
#define TK_NOT '!'
#define TK_EQEQ 256
#define TK_LEQ  257
#define TK_GEQ  258
#define TK_NOTEQ 259
#define TK_AND '&'
#define TK_ANDAND 260
#define TK_OR '|'
#define TK_OROR 261
#define TK_STRING 262
#define TK_DIGIT 263
#define TK_NAME  264
#define TK_INT   0x200
#define TK_FLOAT 0x201
#define TK_FOPEN  '('
#define TK_FCLOSE ')'
#define TK_ERROR -2
#define TK_EOF -1

// forwarder ...
static bool run_cond_1(const char *);
static int tk_lex_func(const char *&c, std::string &tkn);

static int cur_lineno_prepro = 1;

// lexical routine to parse condition strings
static int tk_lex(const char *&c, std::string &tk)
{
  tk = "";
  while (*c != 0 && *c <= ' ') { if (*c == '\n') cur_lineno_prepro++; ++c; }
  if (*c == 0)
    return -1;
  tk += *c++;
  switch (c[-1])
  {
    case '=':
      if (*c == '=') { tk += *c++; return TK_EQEQ; }
      return TK_EQ;
    case '!':
      if (*c == '=') { tk += *c++; return TK_NOTEQ; }
      return TK_NOT;
    case '<':
      if (*c == '=') { tk += *c++; return TK_LEQ; }
      if (*c == '>') { tk += *c++; return TK_NOTEQ; }
      return TK_L;
    case '>':
      if (*c == '=') { tk += *c++; return TK_GEQ; }
      return TK_G;
    case '(': return TK_FOPEN;
    case ')': return TK_FCLOSE;
    case '&':
      if (*c == '&') { tk += *c++; return TK_ANDAND; }
      return TK_AND;
    case '|':
      if (*c == '|') { tk += *c++; return TK_OROR; }
      return TK_OR;
    case '"':
      tk = "";
      while (*c != 0 && *c != '"')
      {
	tk += *c++;
	if (c[-1] == '\\' && *c != 0) tk += *c++;
      }
      if ( *c != '"')
      {
        msg_error("Unterminated string \"%s\" found near line.\n", tk.c_str(), cur_lineno_prepro);
	      return TK_ERROR;
      }
      ++c;
      return TK_STRING;
    case '\'':
      tk = "";
      while (*c != 0 && *c != '\'')
      {
	tk += *c++;
	if (c[-1] == '\\' && *c != 0) tk += *c++;
      }
      if ( *c != '\'' )
      {
        msg_error("Unterminated string '%s' found near line %d.\n", tk.c_str(), cur_lineno_prepro);
	      return TK_ERROR;
      }
      ++c;
      return TK_STRING;
    default:
      if (c[-1] >= '0' && c[-1] <= '9')
      {
        while (*c >= '0' && *c <= '9') tk+=*c++;
        if ((*c != '-' && *c != '_' && *c != '$' && !(*c >= 'a' && *c <= 'z') && !(*c >='A' && *c<='Z')) || *c == '.')
          return TK_DIGIT;
        // fall through
      }
      if ((c[-1] >= 'a' && c[-1] <= 'z') || (c[-1] >= 'A' && c[-1] <= 'Z') || (c[-1] >= '0' && c[-1] <= '9')
	    || c[-1] == '_' || c[-1] == '$' || c[-1] == '.')
	{
	   while ((*c >='a' && *c <='z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_'
	     || *c == '$' || *c == '-' || *c == '.')
	   {
	     tk += *c++;
	   }
	   if ( tk == "and" ) return TK_ANDAND;
	   if ( tk == "or") return TK_OROR;
	   if ( tk == "not") return TK_NOT;
     if (*c == '(')
     {
      if ( tk == "int") return TK_INT;
      if ( tk == "float") return TK_FLOAT;
     }
	   return TK_NAME;
	}
    break;
  }
  msg_error("illegal character '%s' found within condition near line %d.\n", tk.c_str(), cur_lineno_prepro);
  return TK_ERROR;
}

// wrapper function to convert a string object to a C-string and pass it to run_cond_1
bool run_cond(const std::string &cond)
{
  return run_cond_1(cond.c_str());
}

// check if next token is a comparison operator
// returns on error -1, otherwise the token id
static int probe_compare(const char *&c, int &tk2, std::string &tkn2)
{
  std::string tkn;
  const char *sv = c; // save current postion
  int svl = cur_lineno_prepro;

  int tk = tk_lex(c, tkn);
  if ( tk == TK_EQEQ || tk == TK_NOTEQ || tk == TK_G || tk == TK_L || tk == TK_LEQ || tk == TK_GEQ)
  {
    tk2 = tk_lex_func(c, tkn2);
    if ( tk2 == TK_STRING || tk2 == TK_DIGIT || tk2 == TK_NAME)
      return tk;
  }
  c = sv; // restore old position
  cur_lineno_prepro = svl;
  return -1;
}

// forwarder ...
static int eval_tok(const std::string &name, std::string &val);

// evaluate a name token
static int eval_name(const std::string &name, int tok)
{
  if ( tok == TK_DIGIT )
    return atoi(name.c_str()) != 0;
  if ( tok == TK_STRING )
    return 0;

  std::string v;
  tok = eval_tok(name, v);
  if ( tok != TK_DIGIT )
    return 0;
  return atoi(v.c_str()) != 0;
}

// evaluate the parsed token
// We check for values in the cbc, and in the architecture config
static int eval_tok(const std::string &name, std::string &val)
{
  // check for true/True,false/False constants
  if ( name == "true" || name == "True") { val="1"; return TK_DIGIT; }
  if ( name == "false" || name == "False" ) { val="0"; return TK_DIGIT; }
  // do key-value replace
  val = cr_get_cbc_str(name.c_str());
  if ( val == "" )
    val = cr_get_cfg_str(name.c_str());

  if ( val == "" || val == "false" || val == "False")
    val = "0";
  else if ( val == "true" || val == "True" )
    val = "1";

  return TK_DIGIT;
}

// forwarder ...
static int eval_cond(const char *&c);

// read token or function replace
static int tk_lex_func(const char *&c, std::string &tkn)
{
  int tk = tk_lex(c, tkn);
  if ( tk == TK_FLOAT || tk == TK_INT)
  {
    //int tkf = tk;
    std::string tkn2;
    int tk2 = tk_lex(c, tkn2);
    if ( tk2 != TK_FOPEN )
      return -1;
    tk = tk_lex_func(c, tkn);
    if (tk < 0)
      return -1;
    tk2 = tk_lex(c, tkn2);
    if ( tk2 != TK_FCLOSE )
      return -1;
    // TODO implement real conversion if (TK_INT)
  }
  return tk;
}

// evaluate an unary expression in condition string
// Returns 0, 1, or a negative value on error
static int eval_string(const char *&c)
{
  const char *sv = c; // save current position
  int svl = cur_lineno_prepro;
  std::string tkn;

  int tk = tk_lex_func(c, tkn);
  if ( tk == TK_NOT)
  {
    int r = eval_string(c);
    // on error, pass it back
    if (r < 0) { c = sv; cur_lineno_prepro = svl; return r; }
    // toggle the condition
    return r ^ 1;
  }
  // is this a open frame?
  if ( tk == TK_FOPEN )
  {
    int r = eval_cond(c);
    // pass back errors
    if ( r < 0 ) { c = sv; return r; }

    sv = c;
    svl = cur_lineno_prepro;
    tk = tk_lex(c, tkn);
    // check if we have a fitting end frame
    if ( tk != TK_FCLOSE)
    {
      c = sv;
      cur_lineno_prepro = svl;
      return -1;
    }
    return r;
  }

  // is this an expected unary token?
  if ( tk != TK_STRING && tk != TK_DIGIT && tk != TK_NAME)
    return -1;

  std::string r1, r2;
  std::string tkn2;
  int tk2, tk3;

  // check if we see as next token a compare
  tk2 = probe_compare(c, tk3, tkn2);
  if (tk2 < 0 )
    return eval_name(tkn, tk); // no, evaluate the name

  // perform the compare magic
  if (tk != TK_DIGIT && tk != TK_STRING)
    tk = eval_tok(tkn, r1);
  else
    r1 = tkn;
  if (tk3 != TK_DIGIT && tk3 != TK_STRING)
    tk3 = eval_tok(tkn2, r2);
  else
     r2 = tkn2;
  if (tk != TK_DIGIT && tk3 != TK_DIGIT)
  {
    switch (tk2)
    {
      case TK_EQEQ: return r1 == r2 ? 1 : 0;
      case TK_NOTEQ: return r1 != r2 ? 1 : 0;
      case TK_L: return strcmp(r1.c_str(), r2.c_str()) < 0 ? 1 : 0;
      case TK_LEQ: return strcmp(r1.c_str(), r2.c_str()) <= 0 ? 1 : 0;
      case TK_G: return strcmp(r1.c_str(), r2.c_str()) > 0 ? 1 : 0;
      case TK_GEQ: return strcmp(r1.c_str(), r2.c_str()) >= 0 ? 1 : 0;
      default:
        msg_error("need to eval compare of %s %d %s near line %d\n", r1.c_str(), tk2, r2.c_str(), cur_lineno_prepro);
        return 0;
    }
  }
  int l = atoi(r1.c_str());
  int r = atoi(r2.c_str());

  switch (tk2)
  {
    case TK_EQEQ: return l==r ? 1 : 0;
    case TK_NOTEQ: return l!=r ? 1 : 0;
    case TK_L: return l < r ? 1 : 0;
    case TK_G: return l > r ? 1 : 0;
    case TK_LEQ: return l <= r ? 1 : 0;
    case TK_GEQ: return l >= r ? 1 : 0;
    default:
      break;
  }
  msg_error("need to eval compare of %s %d %s near line %d\n", r1.c_str(), tk2, r2.c_str(), cur_lineno_prepro);
  return 0;
}

// evaluate the condition and return 0, 1, or a negative value on error
static int eval_cond(const char *&c)
{
  const char *sv = c; // save current string position
  int svl = cur_lineno_prepro;

  int r = eval_string(c);
  if (r < 0)
  {
    c = sv; // restore saved position
    cur_lineno_prepro = svl;
    return r;
  }

  while (1)
  {
    sv = c; // save new position
    svl = cur_lineno_prepro;

    std::string tkn;
    int tk = tk_lex(c, tkn);

    // have we reached end of string or frame?
    if ( tk == TK_FCLOSE || tk == TK_EOF)
    {
      c = sv; // restore position
      break;
    }
    // if token is neither logical AND or OR, return with a failure
    if ( tk != TK_ANDAND && tk != TK_OROR )
    {
      c = sv;
      return -1;
    }

    int r2 = eval_string(c);
    if (r2 < 0)
    {
      c = sv;
      return r;
    }
    if (tk == TK_ANDAND)
      r &= r2;
    else
      r |= r2;
  }
  return r;
}

// helper to run a condition
static bool run_cond_1(const char *c)
{
  return eval_cond(c) != 0;
}

// do a whitespace right-hand-trim for string object
inline std::string rtrim(std::string &str)
{
  str.erase(str.find_last_not_of(" \t\r")+1);
  return str;
}

// do a whitespace trim for string object
inline std::string trim(std::string &str)
{
  str.erase(0, str.find_first_not_of(" \r\t"));
  str.erase(str.find_last_not_of(" \t\r")+1);
  return str;
}

// check if given condition is true.
// if there is no valid python-like condition in [...], result is true
static bool is_cond_true(const char *c, int lno)
{
  // eat whitespaces
  while (*c > 0 && *c <=0x20)
    ++c;
  // check we start with [
  if (*c != '[')
    return true;
  // is there a terminating ] to be found?
  if (strchr(c, ']') == NULL)
    return true;
  // eat the [ silently
  ++c;
  // eat whitespaces
  while (*c > 0 && *c <= ' ')
	  ++c;
  // is this an empty []
  if (*c == ']')
	  return true;
  // get comment line until terminating ]
  std::string co;
  while (*c != ']')
	  co += *c++;
  ++c;
  // eat whitespaces
  while (*c > 0 && *c <= ' ')
    ++c;
  if (*c != 0)
    msg_warn("Garbage '%s' found in conditional line at %d. ignored.\n", c, lno);

  // time condition input
  trim(co);
  // std::cout << "found condition " << co << std::endl;
  return run_cond(co);
}

// add a line to destination, if it is not empty or its optional condition is true
static void add_ln(const std::string &l, const std::string &c, std::string &dst, int lno)
{
  if (l.length() <= 0)
    return;
  if (!c.length() || is_cond_true(c.c_str(), lno))
  {
    dst += l + '\n';
    return;
  }
}

// Preprocess a line.  additional it eats comments silently and check
// if in comment a python-like condition is present and handle it, if so.
int preprocess_lines(const std::string &src, std::string &dst)
{
  const char *s = src.c_str();
  cur_lineno_prepro = 1;
  std::string cmt = "";
  std::string lnt = "";
  char quotech = 0;
  int string_start_at = 0;

  while (*s != 0)
  {
    // scan left hand side of line before comment
    // allow # within string constants
    if ((*s != '#' || quotech != 0) && *s != '\n')
    {
      // within strings we treat backslash to escape following character
      // but if it is a newline ...
      if (quotech != 0 && *s == '\\')
      {
        lnt += *s++;
        if (*s != 0 && *s != '\n')
          lnt += *s++;
        continue;
      }
      // if we see a quote sign, check if we need to toggle state
      if ((*s == '"' || *s == '\'') && (quotech == 0 || quotech == *s))
         quotech ^= *s;
      if (quotech != 0) string_start_at = cur_lineno_prepro;
      lnt += *s++;
      continue;
    }
    if (*s == '\n') {
      if (quotech != 0)
      {
        // disable this message as we don't pass YAML's text
        // msg_error("unterminated string constant <%c> in yaml file near line %d. Ignored\n", quotech, string_start_at);
        quotech = 0;
      }
      ++cur_lineno_prepro;
    }
    // scan right hand side, which is comment
    rtrim(lnt);
    if (*s == '#')
    {
      s++;
      while (*s != '\n' && *s != 0)
      {
	cmt += *s++;
      }
      trim(cmt);
    }
    if (*s == '\n')
    {
      ++s;
      add_ln(lnt, cmt, dst, cur_lineno_prepro);
      cmt = "";
      lnt = "";
    }
  }
  if ( lnt.length() > 0 )
    add_ln(lnt, cmt, dst, cur_lineno_prepro);
  return 0;
}

bool rdtext(const char *fname, std::string &dst)
{
  dst = "";
  char s[1024];
  FILE *fp = fopen(fname, "rb");
  if (!fp) return false;
  size_t l;
  while((l = fread(s,1,1023, fp)) > 0)
  {
     s[l] = 0;
     dst += s;
  }
  fclose(fp);
  return true;
}
