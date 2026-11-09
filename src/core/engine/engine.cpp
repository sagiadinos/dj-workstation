// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/engine.hpp"

#include <algorithm>
#include <variant>

namespace djw::core::engine
{
    namespace
    {
        template <typename... Ts> struct Overloaded : Ts...
        {
                using Ts::operator()...;
        };
    } // namespace

    Engine::DeckSlot::DeckSlot(api::DeckId id, AudioSource &source)
        : deck{id, source}, snapshot{api::DeckSnapshot{.deck = id}}
    {
    }

    Engine::Engine(EngineConfig config) : config{config}, deck_buffer(config.max_block_frames * stereo_channels) {}

    api::CommandQueue &Engine::addCommandQueue()
    {
        command_queues.push_back(std::make_unique<api::CommandQueue>());
        return *command_queues.back();
    }

    api::DeckId Engine::addDeck(AudioSource &source)
    {
        const api::DeckId id{next_deck_id++};
        decks.push_back(std::make_unique<DeckSlot>(id, source));
        return id;
    }

    api::DeckSnapshotBuffer *Engine::deckSnapshot(api::DeckId deck) noexcept
    {
        DeckSlot *slot = findDeck(deck);
        return slot != nullptr ? &slot->snapshot : nullptr;
    }

    void Engine::render(std::span<float> output) noexcept // [audio-thread]
    {
        applyCommands();

        const std::size_t block_samples = config.max_block_frames * stereo_channels;
        for (std::size_t offset = 0; offset < output.size(); offset += block_samples)
        {
            renderBlock(output.subspan(offset, std::min(block_samples, output.size() - offset)));
        }

        publishSnapshots();
    }

    Engine::DeckSlot *Engine::findDeck(api::DeckId deck) noexcept
    {
        const auto found = std::ranges::find_if(decks, [deck](const auto &slot) { return slot->deck.id() == deck; });
        return found != decks.end() ? found->get() : nullptr;
    }

    void Engine::applyCommands() noexcept // [audio-thread]
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
                apply(*command);
            }
        }
    }

    void Engine::apply(const api::Command &command) noexcept // [audio-thread]
    {
        std::visit(
            Overloaded{
                [this](const api::DeckPlay &play)
                {
                    if (DeckSlot *slot = findDeck(play.deck))
                    {
                        slot->deck.play();
                    }
                },
                [this](const api::DeckPause &pause)
                {
                    if (DeckSlot *slot = findDeck(pause.deck))
                    {
                        slot->deck.pause();
                    }
                },
                [this](const api::MixerSetChannelVolume &volume)
                {
                    if (DeckSlot *slot = findDeck(volume.deck))
                    {
                        slot->channel.setVolume(volume.value);
                    }
                },
            },
            command);
    }

    void Engine::renderBlock(std::span<float> output) noexcept // [audio-thread]
    {
        std::ranges::fill(output, 0.0F);
        const std::span<float> deck_output = std::span{deck_buffer}.first(output.size());
        for (const auto &slot : decks)
        {
            slot->deck.render(deck_output);
            slot->channel.mixInto(deck_output, output);
        }
    }

    void Engine::publishSnapshots() noexcept // [audio-thread]
    {
        for (const auto &slot : decks)
        {
            slot->snapshot.write(api::DeckSnapshot{
                .deck             = slot->deck.id(),
                .play_state       = slot->deck.isPlaying() ? api::PlayState::Playing : api::PlayState::Stopped,
                .channel_volume   = slot->channel.volume(),
                .position_seconds = static_cast<double>(slot->deck.positionFrames()) / config.sample_rate,
            });
        }
    }
} // namespace djw::core::engine
