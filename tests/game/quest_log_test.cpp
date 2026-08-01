// SPDX-License-Identifier: MIT
//
// Unit tests for the QuestLog system (the journal backend wired up per
// docs/quest-journal.md). Pins the load + state-transition + filtering
// behavior that the JournalWidget and the event-engine bridge rely on.
#include <catch2/catch_test_macros.hpp>

#include "../../src/formats/quests_parser.hpp"
#include "../../src/game/quest_log.hpp"
#include "../../src/util/ilogger.hpp"

using namespace runeharbor::game;
using namespace runeharbor::formats;

namespace
{

class NullLogger : public runeharbor::util::ILogger
{
  public:
    void log(runeharbor::util::LogLevel, std::string_view) override {}
};

runeharbor::formats::QuestEntry makeQuestDef(int qBit, const std::string& text,
                                             const std::string& owner = "Quest Giver")
{
    runeharbor::formats::QuestEntry q;
    q.qBit = qBit;
    q.questNoteText = text;
    q.owner = owner;
    return q;
}

} // namespace

TEST_CASE("QuestLog loadQuestData populates the catalog", "[game][quest]")
{
    NullLogger logger;
    QuestLog log(logger);
    log.loadQuestData({makeQuestDef(1, "Find the chalice"), makeQuestDef(2, "Slay the dragon")});

    REQUIRE(log.totalQuestCount() == 2); // both start Unknown -> counted in "all"
    REQUIRE(log.activeQuestCount() == 0);
    REQUIRE(log.getQuest(1) != nullptr);
    REQUIRE(log.getQuest(1)->text == "Find the chalice");
}

TEST_CASE("startQuest moves Unknown -> Active and records a journal entry", "[game][quest]")
{
    NullLogger logger;
    QuestLog log(logger);
    log.loadQuestData({makeQuestDef(5, "Deliver the letter")});

    REQUIRE(log.startQuest(5, 1000));
    REQUIRE(log.getQuestState(5) == QuestState::Active);
    REQUIRE(log.isQuestActive(5));
    REQUIRE(log.activeQuestCount() == 1);

    // A journal entry is recorded on acceptance.
    const auto& journal = log.getJournal();
    REQUIRE_FALSE(journal.empty());
    REQUIRE(journal.back().questBit == 5);
}

TEST_CASE("startQuest dedupes (second call is a no-op)", "[game][quest]")
{
    NullLogger logger;
    QuestLog log(logger);
    log.loadQuestData({makeQuestDef(7, "Unique quest")});

    REQUIRE(log.startQuest(7, 100));
    REQUIRE_FALSE(log.startQuest(7, 200)); // already Active -> no-op, returns false
    REQUIRE(log.activeQuestCount() == 1);
    // Journal should only have the one acceptance entry.
    REQUIRE(log.getJournal().size() == 1);
}

TEST_CASE("completeQuest moves Active -> Completed", "[game][quest]")
{
    NullLogger logger;
    QuestLog log(logger);
    log.loadQuestData({makeQuestDef(3, "Active quest")});
    log.startQuest(3, 0);

    REQUIRE(log.completeQuest(3, 5000));
    REQUIRE(log.getQuestState(3) == QuestState::Completed);
    REQUIRE(log.isQuestCompleted(3));
    REQUIRE(log.completedQuestCount() == 1);
    REQUIRE(log.activeQuestCount() == 0);
    REQUIRE_FALSE(log.getActiveQuests().empty() == false); // active list empty
    REQUIRE_FALSE(log.getCompletedQuests().empty());
}

TEST_CASE("completeQuest refuses a quest that was never started", "[game][quest]")
{
    NullLogger logger;
    QuestLog log(logger);
    log.loadQuestData({makeQuestDef(9, "Never started")});

    REQUIRE_FALSE(log.completeQuest(9, 0));
    REQUIRE(log.getQuestState(9) == QuestState::Unknown);
}

TEST_CASE("startQuest auto-creates an entry for an unknown quest bit", "[game][quest]")
{
    // The event-engine bridge fires startQuest for any varIndex, including ones
    // not in quests.txt (MM7 has many more quest bits than text rows).
    NullLogger logger;
    QuestLog log(logger);
    log.loadQuestData({}); // empty catalog

    REQUIRE(log.startQuest(42, 0));
    REQUIRE(log.getQuestState(42) == QuestState::Active);
    REQUIRE(log.getQuest(42) != nullptr); // synthetic entry created
}

TEST_CASE("failQuest moves Active -> Failed", "[game][quest]")
{
    NullLogger logger;
    QuestLog log(logger);
    log.loadQuestData({makeQuestDef(11, "Timed quest")});
    log.startQuest(11, 0);

    REQUIRE(log.failQuest(11, 9000));
    REQUIRE(log.getQuestState(11) == QuestState::Failed);
    REQUIRE(log.activeQuestCount() == 0);
    // Failed quests appear in getAllQuests (no separate failed-list accessor).
    bool found = false;
    for (const auto* q : log.getAllQuests())
    {
        if (q->questBit == 11)
            found = true;
    }
    REQUIRE(found);
}
