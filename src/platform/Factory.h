#pragma once
#include "core/IActivityProvider.h"
#include <memory>
namespace pcat { std::unique_ptr<IActivityProvider> createActivityProvider(); }
