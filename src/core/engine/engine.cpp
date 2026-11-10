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
        static_assert(std::variant_size_v<api::Command> == 15, "handle the new command in Engine::apply");
    } // namespace

    Engine::DeckSlot::DeckSlot(api::DeckId id, std::uint32_t sample_rate)
        : deck{id, static_cast<std::size_t>(pause_fade_seconds * sample_rate)}, channel{sample_rate},
          snapshot{api::DeckSnapshot{.deck = id}}
    {
    }

    Engine::Engine(EngineConfig config)
        : config{config}, deck_buffer(config.max_block_frames * stereo_channels),
          master_buffer(config.max_block_frames * stereo_channels),
          cue_buffer(config.max_block_frames * stereo_channels),
          headphone_buffer(config.max_block_frames * stereo_channels), master{config.sample_rate},
          headphones{config.sample_rate},
          mixer_snapshot{
              api::MixerSnapshot{.headphones_available = config.output_channels >= headphone_output_channels}},
          current_list{std::make_unique<DeckList>()}
    {
        this->config.output_channels = std::max<std::size_t>(this->config.output_channels, stereo_channels);
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
        slots.push_back(std::make_unique<DeckSlot>(id, config.sample_rate));
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

        const std::size_t channels = config.output_channels;
        const std::size_t frames   = output.size() / channels;
        for (std::size_t done = 0; done < frames; done += config.max_block_frames)
        {
            const std::size_t block = std::min(config.max_block_frames, frames - done);
            renderBlock(list, block);
            routeOutput({.master     = std::span{master_buffer}.first(block * stereo_channels),
                         .headphones = std::span{headphone_buffer}.first(block * stereo_channels)},
                        output.subspan(done * channels, block * channels), channels);
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

    template <typename DeckCommand, typename Action>
    bool Engine::applyToDeck(const DeckList &list, const api::Command &command,
                             Action action) noexcept // [audio-thread]
    {
        const auto *deck_command = std::get_if<DeckCommand>(&command);
        if (deck_command == nullptr)
        {
            return false;
        }
        if (DeckSlot *slot = findDeck(list, deck_command->deck))
        {
            action(*slot, *deck_command);
        }
        return true;
    }

    void Engine::apply(const DeckList &list, const api::Command &command) noexcept // [audio-thread]
    {
        // std::get_if instead of std::visit: std::visit may throw, which the audio thread must not
        const bool deck_command =
            applyToDeck<api::DeckPlay>(list, command, [](DeckSlot &slot, const auto &) { slot.deck.play(); }) ||
            applyToDeck<api::DeckPause>(list, command, [](DeckSlot &slot, const auto &) { slot.deck.pause(); }) ||
            applyToDeck<api::DeckCue>(list, command,
                                      [](DeckSlot &slot, const api::DeckCue &cue) { slot.deck.cue(cue.pressed); }) ||
            applyToDeck<api::DeckSetPitch>(list, command, [](DeckSlot &slot, const api::DeckSetPitch &pitch)
                                           { slot.deck.setPitch(pitch.percent); }) ||
            applyToDeck<api::DeckPitchBend>(list, command, [](DeckSlot &slot, const api::DeckPitchBend &bend)
                                            { slot.deck.setPitchBend(bend.direction); }) ||
            applyToDeck<api::DeckSeek>(list, command,
                                       [rate = config.sample_rate](DeckSlot &slot, const api::DeckSeek &seek)
                                       {
                                           const double frame = std::max(0.0, seek.position_seconds) * rate;
                                           slot.deck.seek(static_cast<std::uint64_t>(std::llround(frame)));
                                       }) ||
            applyToDeck<api::MixerSetChannelVolume>(list, command,
                                                    [](DeckSlot &slot, const api::MixerSetChannelVolume &volume)
                                                    { slot.channel.setVolume(volume.value); }) ||
            applyToDeck<api::MixerAssignCrossfader>(list, command,
                                                    [](DeckSlot &slot, const api::MixerAssignCrossfader &assign)
                                                    { slot.channel.setCrossfaderSide(assign.side); }) ||
            applyToDeck<api::MixerSetHeadphoneCue>(list, command,
                                                   [](DeckSlot &slot, const api::MixerSetHeadphoneCue &cue)
                                                   { slot.channel.setHeadphoneCue(cue.enabled); }) ||
            applyToDeck<api::MixerSetHeadphoneLevel>(list, command,
                                                     [](DeckSlot &slot, const api::MixerSetHeadphoneLevel &level)
                                                     { slot.channel.setHeadphoneLevel(level.value); });
        if (!deck_command)
        {
            applyMixer(command);
        }
    }

    void Engine::applyMixer(const api::Command &command) noexcept // [audio-thread]
    {
        if (const auto *position = std::get_if<api::MixerSetCrossfader>(&command))
        {
            crossfader.setPosition(position->position);
        }
        else if (const auto *momentary = std::get_if<api::MixerCrossfaderMomentary>(&command))
        {
            crossfader.setMomentary(momentary->side);
        }
        else if (const auto *volume = std::get_if<api::MixerSetMasterVolume>(&command))
        {
            master.setVolume(volume->value);
        }
        else if (const auto *cue_mix = std::get_if<api::MixerSetCueMix>(&command))
        {
            headphones.setCueMix(cue_mix->value);
        }
        else if (const auto *headphone_volume = std::get_if<api::MixerSetHeadphoneVolume>(&command))
        {
            headphones.setVolume(headphone_volume->value);
        }
    }

    void Engine::renderBlock(const DeckList &list, std::size_t frames) noexcept // [audio-thread]
    {
        const std::size_t      samples     = frames * stereo_channels;
        const std::span<float> deck_output = std::span{deck_buffer}.first(samples);
        const std::span<float> master_sum  = std::span{master_buffer}.first(samples);
        const std::span<float> cue_sum     = std::span{cue_buffer}.first(samples);
        std::ranges::fill(master_sum, 0.0F);
        std::ranges::fill(cue_sum, 0.0F);
        for (DeckSlot *slot : list.slots)
        {
            slot->deck.render(deck_output);
            slot->channel.mixInto(deck_output, {.master = master_sum, .cue = cue_sum}, crossfader);
        }
        master.process(master_sum);
        if (headphonesAvailable())
        {
            headphones.process({.cue = cue_sum, .master = master_sum}, std::span{headphone_buffer}.first(samples));
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
                .crossfader_side  = slot->channel.crossfaderSide(),
                .level            = slot->channel.level(),
                .headphone_cue    = slot->channel.headphoneCue(),
                .headphone_level  = slot->channel.headphoneLevel(),
            });
        }
        mixer_snapshot.write(api::MixerSnapshot{
            .crossfader_position  = crossfader.position(),
            .crossfader_momentary = crossfader.momentary(),
            .master_volume        = master.volume(),
            .master_level         = master.level(),
            .limiter_gain         = master.limiterGain(),
            .headphones_available = headphonesAvailable(),
            .cue_mix              = headphones.cueMix(),
            .headphone_volume     = headphones.volume(),
        });
    }
} // namespace djw::core::engine
