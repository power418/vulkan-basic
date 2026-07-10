#pragma once
#include "window.hpp"
#include <memory>
#include <string>

std::unique_ptr<Window> createPlatformWindow(const std::string& mode);
