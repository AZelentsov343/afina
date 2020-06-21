#include <afina/Version.h>

#include <string>
#include <sstream>

namespace Afina {

const std::string Version_Major = "";
const std::string Version_Minor = "";
const std::string Version_Patch = "";
const std::string Version_Release = "";
const std::string Version_SHA =  "";

std::string get_version() {
  std::stringstream app_string;
  app_string << "Afina " << Afina::Version_Major << "." << Afina::Version_Minor << "." << Afina::Version_Patch;
  if (Afina::Version_SHA.size() > 0) {
      app_string << "-" << Afina::Version_SHA;
  }
  return app_string.str();
}

} // namespace Afina
