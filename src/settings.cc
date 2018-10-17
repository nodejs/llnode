#include "src/settings.h"
#include "deps/rang/include/rang.hpp"

namespace llnode {

Settings::Settings() {}
Settings::~Settings() {}

Settings Settings::instance;
Settings* Settings::GetSettings() { return &Settings::instance; }

std::string Settings::SetColor(std::string option) {
  if (option == "auto" || option == "always" || option == "never")
    color = option;

  if (ShouldUseColor())
    rang::setControlMode(rang::control::Force);
  else
    rang::setControlMode(rang::control::Off);

  return color;
}

int Settings::SetTreePadding(int option) {
  if (option < 1) option = 1;
  tree_padding = option;
  return tree_padding;
}

bool Settings::ShouldUseColor() {
#ifdef NO_COLOR_OUTPUT
  return false;
#endif
  if (color == "always" ||
      (color == "auto" && rang::rang_implementation::supportsColor() &&
       rang::rang_implementation::isTerminal(std::cout.rdbuf())))
    return true;
  return false;
}

}  // namespace llnode
