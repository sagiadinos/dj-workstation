// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/message_format.hpp"

#include <algorithm>
#include <charconv>
#include <optional>
#include <vector>

namespace djw::player_ui::sdl
{
    namespace
    {
        using Result = std::expected<void, std::string>;

        std::string_view trim(std::string_view text)
        {
            constexpr std::string_view blanks = " \t\r\n";
            const std::size_t          first  = text.find_first_not_of(blanks);
            if (first == std::string_view::npos)
            {
                return {};
            }
            return text.substr(first, text.find_last_not_of(blanks) - first + 1);
        }

        /// True if the apostrophe at text[at] starts a quoted section ('{' or '}')
        bool startsQuote(std::string_view text, std::size_t at)
        {
            return at + 1 < text.size() && (text[at + 1] == '{' || text[at + 1] == '}');
        }

        /// The index after the quoted section that starts with the apostrophe at text[at]
        std::size_t skipQuote(std::string_view text, std::size_t at)
        {
            std::size_t index = at + 1;
            while (index < text.size())
            {
                if (text[index] == '\'')
                {
                    if (index + 1 < text.size() && text[index + 1] == '\'')
                    {
                        index += 2; // an apostrophe inside the quote
                        continue;
                    }
                    return index + 1;
                }
                ++index;
            }
            return text.size();
        }

        /// The index of the } that closes the { at text[open]; std::nullopt if there is none
        std::optional<std::size_t> matchingBrace(std::string_view text, std::size_t open)
        {
            int         depth = 1;
            std::size_t index = open + 1;
            while (index < text.size())
            {
                const char current = text[index];
                if (current == '\'')
                {
                    if (index + 1 < text.size() && text[index + 1] == '\'')
                    {
                        index += 2;
                    }
                    else if (startsQuote(text, index))
                    {
                        index = skipQuote(text, index);
                    }
                    else
                    {
                        ++index;
                    }
                    continue;
                }
                if (current == '{')
                {
                    ++depth;
                }
                else if (current == '}' && --depth == 0)
                {
                    return index;
                }
                ++index;
            }
            return std::nullopt;
        }

        struct Branch
        {
                std::string_view selector;
                std::string_view message;
        };

        class Formatter
        {
            public:
                Formatter(Language language, std::span<const MessageArg> args) : language{language}, args{args} {}

                Result append(std::string &out, std::string_view text, std::optional<long long> hash) const
                {
                    std::size_t index = 0;
                    while (index < text.size())
                    {
                        const char current = text[index];
                        if (current == '\'')
                        {
                            if (index + 1 < text.size() && text[index + 1] == '\'')
                            {
                                out += '\'';
                                index += 2;
                            }
                            else if (startsQuote(text, index))
                            {
                                const std::size_t end = skipQuote(text, index);
                                // The quoted text without its apostrophes; '' inside is one apostrophe
                                for (std::size_t at = index + 1; at < end; ++at)
                                {
                                    if (text[at] == '\'')
                                    {
                                        if (at + 1 < end && text[at + 1] == '\'')
                                        {
                                            out += '\'';
                                            ++at;
                                        }
                                        continue; // the closing apostrophe
                                    }
                                    out += text[at];
                                }
                                index = end;
                            }
                            else
                            {
                                out += '\'';
                                ++index;
                            }
                        }
                        else if (current == '{')
                        {
                            const auto close = matchingBrace(text, index);
                            if (!close)
                            {
                                return std::unexpected{"missing }"};
                            }
                            if (auto done = argument(out, text.substr(index + 1, *close - index - 1)); !done)
                            {
                                return done;
                            }
                            index = *close + 1;
                        }
                        else if (current == '}')
                        {
                            return std::unexpected{"unexpected }"};
                        }
                        else if (current == '#' && hash)
                        {
                            out += std::to_string(*hash);
                            ++index;
                        }
                        else
                        {
                            out += current;
                            ++index;
                        }
                    }
                    return {};
                }

            private:
                [[nodiscard]] const MessageArg *find(std::string_view name) const
                {
                    const auto found = std::ranges::find(args, name, &MessageArg::name);
                    return found != args.end() ? &*found : nullptr;
                }

                /// The inside of one { }: a name, or a name with the plural rule
                Result argument(std::string &out, std::string_view inside) const
                {
                    const std::size_t comma = inside.find(',');
                    const auto        name  = trim(inside.substr(0, comma));
                    const MessageArg *arg   = find(name);
                    if (arg == nullptr)
                    {
                        return std::unexpected{"missing argument " + std::string{name}};
                    }
                    if (comma == std::string_view::npos)
                    {
                        out += arg->isNumber() ? std::to_string(arg->number()) : arg->text();
                        return {};
                    }
                    return plural(out, *arg, inside.substr(comma + 1));
                }

                /// "plural, one {...} other {...}" for the number of arg
                Result plural(std::string &out, const MessageArg &arg, std::string_view rule) const
                {
                    const std::size_t comma = rule.find(',');
                    if (trim(rule.substr(0, comma)) != "plural" || comma == std::string_view::npos)
                    {
                        return std::unexpected{"only plural is supported in " + std::string{arg.name()}};
                    }
                    if (!arg.isNumber())
                    {
                        return std::unexpected{std::string{arg.name()} + " must be a number"};
                    }

                    std::vector<Branch> branches;
                    std::string_view    rest = rule.substr(comma + 1);
                    while (!(rest = trim(rest)).empty())
                    {
                        const std::size_t open = rest.find('{');
                        if (open == std::string_view::npos)
                        {
                            return std::unexpected{"plural branch without {"};
                        }
                        const auto close = matchingBrace(rest, open);
                        if (!close)
                        {
                            return std::unexpected{"plural branch without }"};
                        }
                        branches.push_back({trim(rest.substr(0, open)), rest.substr(open + 1, *close - open - 1)});
                        rest = rest.substr(*close + 1);
                    }

                    const long long        number   = arg.number();
                    const std::string      exact    = '=' + std::to_string(number);
                    const std::string_view category = pluralCategoryName(pluralCategory(language, number));
                    const auto             pick     = [&branches](std::string_view selector) -> const Branch *
                    {
                        const auto found = std::ranges::find(branches, selector, &Branch::selector);
                        return found != branches.end() ? &*found : nullptr;
                    };
                    const Branch *chosen = pick(exact);
                    chosen               = chosen != nullptr ? chosen : pick(category);
                    chosen               = chosen != nullptr ? chosen : pick("other");
                    if (chosen == nullptr)
                    {
                        return std::unexpected{"plural of " + std::string{arg.name()} + " has no other"};
                    }
                    return append(out, chosen->message, number);
                }

                Language                    language;
                std::span<const MessageArg> args;
        };
    } // namespace

    std::expected<std::string, std::string> formatMessage(Language language, std::string_view pattern,
                                                          std::span<const MessageArg> args)
    {
        std::string out;
        if (auto done = Formatter{language, args}.append(out, pattern, std::nullopt); !done)
        {
            return std::unexpected{std::move(done.error())};
        }
        return out;
    }
} // namespace djw::player_ui::sdl
