// SPDX-License-Identifier: MIT
//
// Unit tests for util/string_utils. The primary target is `wordWrap`: it backs
// both the dialogue and journal UIs, and a prior dialogue-only implementation
// silently dropped spaces and never broke long words, overflowing the window.
// These tests pin the shared helper's contract so that regression cannot recur.
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../../src/util/string_utils.hpp"

using namespace runeharbor::util;

// ---------------------------------------------------------------------------
// wordWrap
// ---------------------------------------------------------------------------

TEST_CASE("wordWrap returns no lines for empty input", "[util][string_utils]")
{
    REQUIRE(wordWrap("", 40).empty());
}

TEST_CASE("wordWrap returns no lines for non-positive width", "[util][string_utils]")
{
    REQUIRE(wordWrap("hello world", 0).empty());
    REQUIRE(wordWrap("hello world", -5).empty());
}

TEST_CASE("wordWrap fits short text on a single line", "[util][string_utils]")
{
    const auto lines = wordWrap("hello world", 40);
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0] == "hello world");
}

TEST_CASE("wordWrap breaks at word boundaries", "[util][string_utils]")
{
    // Width 5 fits "hello" exactly; "world" must wrap to its own line.
    const auto lines = wordWrap("hello world", 5);
    REQUIRE(lines.size() == 2);
    REQUIRE(lines[0] == "hello");
    REQUIRE(lines[1] == "world");
}

TEST_CASE("wordWrap preserves the separating space (no dropped words)", "[util][string_utils]")
{
    // Regression guard: the old dialogue wordWrap dropped the space at the break
    // and could lose words. Verify every input word appears in the output.
    const std::string input = "the quick brown fox jumps over the lazy dog";
    const auto lines = wordWrap(input, 10);

    std::string rejoined;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (i > 0)
            rejoined += " ";
        rejoined += lines[i];
    }
    REQUIRE(rejoined == input);
}

TEST_CASE("wordWrap hard-breaks a word longer than the column", "[util][string_utils]")
{
    // A single 10-char word with a 4-char column must split across lines rather
    // than overflow. This is the case the old dialogue implementation missed.
    const auto lines = wordWrap("abcdefghij", 4);
    REQUIRE(lines.size() == 3);
    REQUIRE(lines[0] == "abcd");
    REQUIRE(lines[1] == "efgh");
    REQUIRE(lines[2] == "ij");
}

TEST_CASE("wordWrap hard-breaks a long word mid-paragraph", "[util][string_utils]")
{
    // "supercalifragilistic" (20 chars) in a 6-char column: the long word is
    // chunked into 6-char pieces (with the "ic" remnant on its own line), and
    // the surrounding short words stay intact and are never lost.
    const auto lines = wordWrap("a supercalifragilistic b", 6);
    REQUIRE(lines.size() == 6);
    REQUIRE(lines[0] == "a");
    REQUIRE(lines[1] == "superc");
    REQUIRE(lines[2] == "alifra");
    REQUIRE(lines[3] == "gilist");
    REQUIRE(lines[4] == "ic");
    REQUIRE(lines[5] == "b");
}

TEST_CASE("wordWrap honors explicit newlines", "[util][string_utils]")
{
    const auto lines = wordWrap("line one\nline two", 40);
    REQUIRE(lines.size() == 2);
    REQUIRE(lines[0] == "line one");
    REQUIRE(lines[1] == "line two");
}

TEST_CASE("wordWrap collapses trailing spaces at line starts", "[util][string_utils]")
{
    // A break leaves no leading space on the continuation line.
    const auto lines = wordWrap("alpha beta gamma", 6);
    REQUIRE(lines.size() == 3);
    for (const auto& line : lines)
    {
        REQUIRE_FALSE(line.empty());
        REQUIRE(line.front() != ' ');
        REQUIRE(line.back() != ' ');
    }
}

TEST_CASE("wordWrap width-1 wraps one char per line", "[util][string_utils]")
{
    const auto lines = wordWrap("ab cd", 1);
    REQUIRE(lines.size() == 4);
    REQUIRE(lines[0] == "a");
    REQUIRE(lines[1] == "b");
    REQUIRE(lines[2] == "c");
    REQUIRE(lines[3] == "d");
}

// ---------------------------------------------------------------------------
// trim / cleanNumericString / equalsIgnoreCase / toLower / splitString
// ---------------------------------------------------------------------------

TEST_CASE("trim strips surrounding whitespace", "[util][string_utils]")
{
    REQUIRE(trim("  hello  ") == "hello");
    REQUIRE(trim("\thello\n") == "hello");
    REQUIRE(trim("hello") == "hello");
    REQUIRE(trim("   ") == "");
}

TEST_CASE("cleanNumericString removes spaces and commas", "[util][string_utils]")
{
    REQUIRE(cleanNumericString("1,234,567") == "1234567");
    REQUIRE(cleanNumericString("1 000") == "1000");
    REQUIRE(cleanNumericString("42") == "42");
}

TEST_CASE("equalsIgnoreCase compares ASCII case-insensitively", "[util][string_utils]")
{
    REQUIRE(equalsIgnoreCase("Weapon", "weapon"));
    REQUIRE(equalsIgnoreCase("WEAPON", "weapon"));
    REQUIRE(equalsIgnoreCase("weapon", "WEAPON"));
    REQUIRE_FALSE(equalsIgnoreCase("weapon", "armor"));
    REQUIRE_FALSE(equalsIgnoreCase("weapon", "weapons"));
}

TEST_CASE("toLower lowercases the input", "[util][string_utils]")
{
    REQUIRE(toLower("Hello WORLD") == "hello world");
    REQUIRE(toLower("already lower") == "already lower");
    REQUIRE(toLower("") == "");
}

TEST_CASE("splitString splits on a delimiter", "[util][string_utils]")
{
    const auto parts = splitString("a,b,c", ',');
    REQUIRE(parts.size() == 3);
    REQUIRE(parts[0] == "a");
    REQUIRE(parts[1] == "b");
    REQUIRE(parts[2] == "c");
}

TEST_CASE("splitString handles a quoted delimiter", "[util][string_utils]")
{
    const auto parts = splitString(R"("a,b",c)", ',', '"');
    REQUIRE(parts.size() == 2);
    REQUIRE(parts[0] == "a,b");
    REQUIRE(parts[1] == "c");
}
