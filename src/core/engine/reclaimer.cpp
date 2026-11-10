// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/reclaimer.hpp"

#include <utility>

namespace djw::core::engine
{
    void Reclaimer::retire(std::shared_ptr<void> object)
    {
        retired.push_back({epoch.load(std::memory_order_seq_cst), std::move(object)});
    }

    void Reclaimer::collect()
    {
        const std::uint64_t now = epoch.load(std::memory_order_seq_cst);
        std::erase_if(retired, [now](const Retired &entry) { return now > entry.epoch; });
    }
} // namespace djw::core::engine
