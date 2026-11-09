// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/version.hpp"

#include <iostream>

int main()
{
    std::cout << "djw " << djw::core::api::version() << '\n';
    return 0;
}
