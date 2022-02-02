#ifndef sb_filesystem_executable_path_h
# error "Direct inclusion error."
#endif

/*
   Copyright (c) 2022, Scott Bailey
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
       * Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.
       * Neither the name of the <organization> nor the
         names of its contributors may be used to endorse or promote products
         derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL SCOTT BAILEY BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/



#if defined(__linux__)
# include <linux/limits.h>      // PATH_MAX
# include <string.h>            // memset()
# include <unistd.h>            // readlink()
#elif defined(__APPLE__)
# include <mach-o/dyld.h>       // _NSGetExecutablePath()
#elif defined(_WIN32)
# include <windows.h>
# include <libloaderapi.h>      // GetModuleFileNameW()
# include <errhandlingapi.h>    // GetLastError()
# include <stdlib.h>            // _MAX_PATH
#else
# error "Need to implement for your OS"
#endif




namespace sb {
namespace filesystem {

inline std::filesystem::path executable_path() {

#if defined(__linux__)
  char dest[PATH_MAX];
  memset(dest,0,sizeof(dest)); // readlink does not null terminate!

  if (readlink("/proc/self/exe", dest, PATH_MAX) == -1) {
    perror("readlink");
  }

#elif defined(__APPLE__)
  char dest[PATH_MAX];
  memset(dest,0,sizeof(dest)); // readlink does not null terminate!

  uint32_t size = sizeof(dest);
  if (_NSGetExecutablePath(dest, &size) != 0) {
    perror("_NSGetExecutablePath");
  }

#elif defined(_WIN32)
  wchar_t dest[_MAX_PATH];
  if(!GetModuleFileNameW( NULL, dest, _MAX_PATH )) {
    std::cerr << "GetModuleFileNameW() error: " << GetLastError() << std::endl;
  }

#else
# error "Need to implement for your OS"
#endif

  std::error_code sec;

  // Conversion to an absolute path is probably unnecessary; however this should be called rarely so a small performance hit is
  // acceptable.
  auto rv = std::filesystem::absolute(dest,sec);
  if(sec) {
    std::cerr << "get_executable_path() - absolute() error: " << sec << std::endl;
  }

  // Following the symlink is a requirement!
  if(std::filesystem::is_symlink(rv,sec)) {
    rv = std::filesystem::read_symlink(rv,sec);
  }
  if(sec) {
    std::cerr << "get_executable_path() - read_symlink() error: " << sec << std::endl;
  }

  return rv;
}

} // namespace filesystem
} // namespace sb
