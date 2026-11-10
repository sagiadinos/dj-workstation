// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/engine.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

namespace djw::core::engine
{
    namespace
    {
        // Long enough to avoid a click, short enough to feel instant
        constexpr double pause_fade_seconds = 0.005;

        // Engine::apply handles every command; a new command must be added there
        static_assert(std::variant_size_v<api::Command> == 7, "handle the new command in Engine::apply");
    } // namespace

    Engine::DeckSlot::DeckSlot(api::DeckId id, std::size_t pause_fade_frames)
        : deck{id, pause_fade_frames}, snapshot{api::DeckSnapshot{.deck = id}}
    {
    }

    Engine::Engine(EngineConfig config)
        : config{config}, deck_buffer(config.max_block_frames * stereo_channels),
          current_list{std::make_unique<DeckList>()}
    {
        published_list.store(current_list.get(), std::memory_order_seq_cst);
    }

    // Rendering has stopped when the engine is destroyed; everything retired can go.
    Engine::~Engine() = default;

    api::CommandQueue &Engine::addCommandQueue()
    {
        command_queues.push_back(std::make_unique<api::CommandQueue>());
        return *command_queues.back();
    }

    api::DeckId Engine::addDeck()
    {
        const api::DeckId id{next_deck_id++};
        const auto        pause_fade_frames = static_cast<std::size_t>(pause_fade_seconds * config.sample_rate);
        slots.push_back(std::make_unique<DeckSlot>(id, pause_fade_frames));
        publishDeckList();
        return id;
    }

    bool Engine::removeDeck(api::DeckId deck)
    {
        const auto found = std::ranges::find_if(slots, [deck](const auto &slot) { return slot->deck.id() == deck; });
        if (found == slots.end())
        {
            return false;
        }
        std::shared_ptr<void> removed{std::move(*found)};
        slots.erase(found);
        publishDeckList();
        reclaimer.retire(std::move(removed));
        return true;
    }

    bool Engine::setTrack(api::DeckId deck, std::unique_ptr<AudioSource> track)
    {
        DeckSlot *slot = findOwned(deck);
        if (slot == nullptr)
        {
            return false;
        }
        std::unique_ptr<AudioSource> previous = std::exchange(slot->owned_track, std::move(track));
        slot->track.store(slot->owned_track.get(), std::memory_order_seq_cst);
        if (previous)
        {
            reclaimer.retire(std::shared_ptr<void>{std::move(previous)});
        }
        return true;
    }

    void Engine::collectGarbage()
    {
        reclaimer.collect();
    }

    api::DeckSnapshotBuffer *Engine::deckSnapshot(api::DeckId deck) noexcept
    {
        DeckSlot *slot = findOwned(deck);
        return slot != nullptr ? &slot->snapshot : nullptr;
    }

    Engine::DeckSlot *Engine::findOwned(api::DeckId deck) noexcept
    {
        const auto found = std::ranges::find_if(slots, [deck](const auto &slot) { return slot->deck.id() == deck; });
        return found != slots.end() ? found->get() : nullptr;
    }

    void Engine::publishDeckList()
    {
        auto list = std::make_unique<DeckList>();
        list->slots.reserve(slots.size());
        for (const auto &slot : slots)
        {
            list->slots.push_back(slot.get());
        }
        published_list.store(list.get(), std::memory_order_seq_cst);
        reclaimer.retire(std::shared_ptr<void>{std::exchange(current_list, std::move(list))});
    }

    void Engine::render(std::span<float> output) noexcept // [audio-thread]
    {
        reclaimer.enterRender();
        const DeckList &list = *published_list.load(std::memory_order_seq_cst);
        for (DeckSlot *slot : list.slots)
        {
            slot->deck.attach(slot->track.load(std::memory_order_seq_cst));
        }

        applyCommands(list);

        const std::size_t block_samples = config.max_block_frames * stereo_channels;
        for (std::size_t offset = 0; offset < output.size(); offset += block_samples)
        {
            renderBlock(list, output.subspan(offset, std::min(block_samples, output.size() - offset)));
        }

        publishSnapshots(list);
    }

    Engine::DeckSlot *Engine::findDeck(const DeckList &list, api::DeckId deck) noexcept // [audio-thread]
    {
        const auto found =
            std::ranges::find_if(list.slots, [deck](const DeckSlot *slot) { return slot->deck.id() == deck; });
        return found != list.slots.end() ? *found : nullptr;
    }

    void Engine::applyCommands(const DeckList &list) noexcept // [audio-thread]
    {
        // At most one queue capacity per call, so a producer that keeps pushing cannot stall the audio thread
        for (const auto &queue : command_queues)
        {
            for (std::size_t i = 0; i < api::CommandQueue::capacity(); ++i)
            {
                const auto command = queue->tryPop();
                if (!command)
                {
                    break;
                }
                apply(list, *command);
            }
        }
    }

    void Engine::apply(const DeckList &list, const api::Command &command) noexcept // [audio-thread]
    {
        // std::get_if instead of std::visit: std::visit may throw, which the audio thread must not
        if (const auto *play = std::get_if<api::DeckPlay>(&command))
        {
            if (DeckSlot *slot = findDeck(list, play->deck))
            {
                slot->deck.play();
            }
        }
        else if (const auto *pause = std::get_if<api::DeckPause>(&command))
        {
            if (DeckSlot *slot = findDeck(list, pause->deck))
            {
                slot->deck.pause();
            }
        }
        else if (const auto *cue = std::get_if<api::DeckCue>(&command))
        {
            if (DeckSlot *slot = findDeck(list, cue->deck))
            {
                slot->deck.cue(cue->pressed);
            }
        }
        else if (const auto *pitch = std::get_if<api::DeckSetPitch>(&command))
        {
            if (DeckSlot *slot = findDeck(list, pitch->deck))
            {
                slot->deck.setPitch(pitch->percent);
            }
        }
        else if (const auto *bend = std::get_if<api::DeckPitchBend>(&command))
        {
            if (DeckSlot *slot = findDeck(list, bend->deck))
            {
                slot->deck.setPitchBend(bend->direction);
            }
        }
        else if (const auto *seek = std::get_if<api::DeckSeek>(&command))
        {
            if (DeckSlot *slot = findDeck(list, seek->deck))
            {
                const double frame = std::max(0.0, seek->position_seconds) * config.sample_rate;
                slot->deck.seek(static_cast<std::uint64_t>(std::llround(frame)));
            }
        }
        else if (const auto *volume = std::get_if<api::MixerSetChannelVolume>(&command))
        {
            if (DeckSlot *slot = findDeck(list, volume->deck))
            {
                slot->channel.setVolume(volume->value);
            }
        }
    }

    void Engine::renderBlock(const DeckList &list, std::span<float> output) noexcept // [audio-thread]
    {
        std::ranges::fill(output, 0.0F);
        const std::span<float> deck_output = std::span{deck_buffer}.first(output.size());
        for (DeckSlot *slot : list.slots)
        {
            slot->deck.render(deck_output);
            slot->channel.mixInto(deck_output, output);
        }
    }

    void Engine::publishSnapshots(const DeckList &list) noexcept // [audio-thread]
    {
        const auto rate = static_cast<double>(config.sample_rate);
        for (DeckSlot *slot : list.slots)
        {
            const Deck &deck = slot->deck;
            slot->snapshot.write(api::DeckSnapshot{
                .deck             = deck.id(),
                .track_loaded     = deck.hasTrack(),
                .play_state       = deck.isPlaying() ? api::PlayState::Playing : api::PlayState::Stopped,
                .channel_volume   = slot->channel.volume(),
                .position_seconds = deck.positionFrames() / rate,
                .duration_seconds = static_cast<double>(deck.lengthFrames()) / rate,
                .cue_seconds      = static_cast<double>(deck.cueFrame()) / rate,
                .pitch_percent    = deck.pitchPercent(),
            });
        }
    }
} // namespace djw::core::engine
