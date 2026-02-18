// SPDX-License-Identifier: MIT
#include "quest_log.hpp"

namespace runeharbor::game
{

QuestLog::QuestLog(util::ILogger& logger) : logger_(logger) {}

void QuestLog::loadQuestData(const std::vector<formats::QuestEntry>& quests)
{
    quests_.clear();
    for (const auto& q : quests)
    {
        QuestEntry entry;
        entry.questBit = q.qBit;
        entry.text = q.questNoteText;
        entry.owner = q.owner;
        entry.notes = q.notes;
        entry.state = QuestState::Unknown;
        quests_[q.qBit] = std::move(entry);
    }
    logger_.info("Loaded " + std::to_string(quests.size()) + " quest definitions");
}

bool QuestLog::startQuest(int questBit, uint64_t gameTime)
{
    auto it = quests_.find(questBit);
    if (it == quests_.end())
    {
        // Create a dynamic quest entry if not in the definitions
        QuestEntry entry;
        entry.questBit = questBit;
        entry.text = "Quest #" + std::to_string(questBit);
        entry.state = QuestState::Active;
        entry.startTime = gameTime;
        quests_[questBit] = std::move(entry);
    }
    else
    {
        if (it->second.state == QuestState::Active)
            return false; // Already active
        it->second.state = QuestState::Active;
        it->second.startTime = gameTime;
    }

    logger_.debug("Quest started: " + std::to_string(questBit));
    addJournalEntry("Quest accepted: " + quests_[questBit].text, gameTime, questBit);

    if (callbacks_.onQuestStateChanged)
        callbacks_.onQuestStateChanged(questBit, QuestState::Active);

    return true;
}

bool QuestLog::completeQuest(int questBit, uint64_t gameTime)
{
    auto it = quests_.find(questBit);
    if (it == quests_.end())
        return false;

    if (it->second.state != QuestState::Active)
        return false;

    it->second.state = QuestState::Completed;
    it->second.endTime = gameTime;

    logger_.debug("Quest completed: " + std::to_string(questBit));
    addJournalEntry("Quest completed: " + it->second.text, gameTime, questBit);

    if (callbacks_.onQuestStateChanged)
        callbacks_.onQuestStateChanged(questBit, QuestState::Completed);

    return true;
}

bool QuestLog::failQuest(int questBit, uint64_t gameTime)
{
    auto it = quests_.find(questBit);
    if (it == quests_.end())
        return false;

    if (it->second.state != QuestState::Active)
        return false;

    it->second.state = QuestState::Failed;
    it->second.endTime = gameTime;

    logger_.debug("Quest failed: " + std::to_string(questBit));
    addJournalEntry("Quest failed: " + it->second.text, gameTime, questBit);

    if (callbacks_.onQuestStateChanged)
        callbacks_.onQuestStateChanged(questBit, QuestState::Failed);

    return true;
}

QuestState QuestLog::getQuestState(int questBit) const
{
    auto it = quests_.find(questBit);
    return it != quests_.end() ? it->second.state : QuestState::Unknown;
}

bool QuestLog::isQuestActive(int questBit) const
{
    return getQuestState(questBit) == QuestState::Active;
}

bool QuestLog::isQuestCompleted(int questBit) const
{
    return getQuestState(questBit) == QuestState::Completed;
}

const QuestEntry* QuestLog::getQuest(int questBit) const
{
    auto it = quests_.find(questBit);
    return it != quests_.end() ? &it->second : nullptr;
}

std::vector<const QuestEntry*> QuestLog::getActiveQuests() const
{
    std::vector<const QuestEntry*> result;
    for (const auto& [bit, quest] : quests_)
    {
        if (quest.state == QuestState::Active)
            result.push_back(&quest);
    }
    return result;
}

std::vector<const QuestEntry*> QuestLog::getCompletedQuests() const
{
    std::vector<const QuestEntry*> result;
    for (const auto& [bit, quest] : quests_)
    {
        if (quest.state == QuestState::Completed)
            result.push_back(&quest);
    }
    return result;
}

std::vector<const QuestEntry*> QuestLog::getAllQuests() const
{
    std::vector<const QuestEntry*> result;
    for (const auto& [bit, quest] : quests_)
    {
        if (quest.state != QuestState::Unknown)
            result.push_back(&quest);
    }
    return result;
}

void QuestLog::addJournalEntry(const std::string& text, uint64_t gameTime, int questBit)
{
    JournalEntry entry;
    entry.gameTime = gameTime;
    entry.text = text;
    entry.questBit = questBit;
    journal_.push_back(std::move(entry));

    if (callbacks_.onJournalEntry)
        callbacks_.onJournalEntry(journal_.back());
}

std::vector<const JournalEntry*> QuestLog::getJournalForQuest(int questBit) const
{
    std::vector<const JournalEntry*> result;
    for (const auto& entry : journal_)
    {
        if (entry.questBit == questBit)
            result.push_back(&entry);
    }
    return result;
}

int QuestLog::activeQuestCount() const
{
    int count = 0;
    for (const auto& [bit, quest] : quests_)
    {
        if (quest.state == QuestState::Active)
            count++;
    }
    return count;
}

int QuestLog::completedQuestCount() const
{
    int count = 0;
    for (const auto& [bit, quest] : quests_)
    {
        if (quest.state == QuestState::Completed)
            count++;
    }
    return count;
}

int QuestLog::totalQuestCount() const
{
    return static_cast<int>(quests_.size());
}

void QuestLog::reset()
{
    for (auto& [bit, quest] : quests_)
    {
        quest.state = QuestState::Unknown;
        quest.startTime = 0;
        quest.endTime = 0;
    }
    journal_.clear();
}

} // namespace runeharbor::game
