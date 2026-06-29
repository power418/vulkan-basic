#pragma once
#include "window.hpp"
#include <memory>

std::unique_ptr<Window> createPlatformWindow();
