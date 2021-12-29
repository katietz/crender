#ifndef PREPRO_HXX
#define PREPRO_HXX

bool rdtext(const char *fname, std::string &dst);

int preprocess_lines(const std::string &src, std::string &dst);

extern "C" void msg_error(const char *fmt, ...);
extern "C" void msg_warn(const char *fmt, ...);

#endif

