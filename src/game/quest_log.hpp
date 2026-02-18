// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../formats/quests_parser.hpp"
#include "../util/ilogger.hpp"

namespace runeharbor::game
{

// Quest states
enum class QuestState : uint8_t
{
    Unknown = 0, // Not yet encountered
    Active,      // In progress
    Completed,   // Done
    Failed,      // Failed (timed out, wrong choice, etc.)
};

// A quest entry with runtime state
struct QuestEntry
{
    int questBit = 0;  // Unique quest identifier
    std::string text;  // Quest description
    std::string owner; // Quest giver
    std::string notes; // Additional info
    QuestState state = QuestState::Unknown;
    uint64_t startTime = 0; // Game time when quest was accepted
    uint64_t endTime = 0;   // Game time when quest was completed/failed
};

// Journal entry (chronological record of quest events)
struct JournalEntry
{
    uint64_t gameTime = 0;
    std::string text;
    int questBit = 0; // 0 = general journal entry, >0 = quest-specific
};

// Callbacks for quest UI updates
struct QuestCallbacks
{
    std::function<void(int questBit, QuestState newState)> onQuestStateChanged;
    std::function<void(const JournalEntry& entry)> onJournalEntry;
};

class QuestLog
{
  public:
    explicit QuestLog(util::ILogger& logger);

    void setCallbacks(const QuestCallbacks& callbacks) { callbacks_ = callbacks; }

    // Load quest definitions from parsed data
    void loadQuestData(const std::vector<formats::QuestEntry>& quests);

    // Quest state management
    bool startQuest(int questBit, uint64_t gameTime);
    bool completeQuest(int questBit, uint64_t gameTime);
    bool failQuest(int questBit, uint64_t gameTime);

    // Query quest state
    QuestState getQuestState(int questBit) const;
    bool isQuestActive(int questBit) const;
    bool isQuestCompleted(int questBit) const;
    const QuestEntry* getQuest(int questBit) const;

    // Get filtered lists
    std::vector<const QuestEntry*> getActiveQuests() const;
    std::vector<const QuestEntry*> getCompletedQuests() const;
    std::vector<const QuestEntry*> getAllQuests() const;

    // Journal
    void addJournalEntry(const std::string& text, uint64_t gameTime, int questBit = 0);
    const std::vector<JournalEntry>& getJournal() const { return journal_; }
    std::vector<const JournalEntry*> getJournalForQuest(int questBit) const;

    // Statistics
    int activeQuestCount() const;
    int completedQuestCount() const;
    int totalQuestCount() const;

    // Reset all quest state
    void reset();

  private:
    util::ILogger& logger_;
    QuestCallbacks callbacks_;
    std::unordered_map<int, QuestEntry> quests_;
    std::vector<JournalEntry> journal_;
};

} // namespace runeharbor::game
