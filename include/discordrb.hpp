#pragma once

#include <rice/Class.hpp>
#include <rice/Constructor.hpp>

#ifdef DISCORDRB_DEBUG
#define DRB_DEBUG(x) std::cout << "[DRB-GATEWAY-DEBUG] " << x << std::endl;
#else
#define DRB_DEBUG(x)
#endif