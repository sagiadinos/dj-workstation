// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_RECLAIMER_HPP
#define DJW_CORE_ENGINE_RECLAIMER_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace djw::core::engine
{
    /**
     * Frees objects the audio thread may still be using, but only once it certainly is not.
     *
     * The control thread first publishes the replacement (e.g. a new deck list through an atomic pointer), then
     * retires the old object. The audio thread calls enterRender() at the start of every render, before it reads
     * any published pointer. An object retired while render n was running is freed as soon as render n + 1 has
     * started: render n is then finished, and render n + 1 already sees the replacement. The audio thread never
     * frees anything and never waits.
     *
     * Without rendering nothing is freed until the Reclaimer itself is destroyed.
     */
    class Reclaimer
    {
        public:
            /// Audio thread: start of a render, before reading published pointers
            void enterRender() noexcept { epoch.fetch_add(1, std::memory_order_seq_cst); } // [audio-thread]

            /// Control thread: keeps object alive until the audio thread can no longer see it. Call after the
            /// replacement has been published.
            void retire(std::shared_ptr<void> object);

            /// Control thread: frees what the audio thread can no longer see.
            void collect();

            /// Control thread: number of objects waiting to be freed
            [[nodiscard]] std::size_t pending() const noexcept { return retired.size(); }

        private:
            struct Retired
            {
                    std::uint64_t         epoch;
                    std::shared_ptr<void> object;
            };

            std::atomic<std::uint64_t> epoch{0};
            std::vector<Retired>       retired;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_RECLAIMER_HPP
