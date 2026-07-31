// SPDX-License-Identifier: MIT
#include "merchant_parser.hpp"

#include <format>
#include <sstream>

#include "../util/string_utils.hpp"

namespace runeharbor::formats
{

MerchantTextParser::MerchantTextParser(util::ILogger& logger) : logger_(logger) {}

bool MerchantTextParser::parse(const std::vector<uint8_t>& data)
{
    rows_.clear();

    if (data.empty())
    {
        logger_.warning("Merchant text data is empty (shopkeeper flavor text disabled)");
        return false;
    }

    const std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line))
    {
        const std::string trimmed = util::trim(line);
        if (trimmed.empty())
        {
            continue;
        }

        std::vector<std::string> fields = util::splitString(line, '\t', '"');
        if (fields.empty())
        {
            continue;
        }

        // The first field is the row label ("Buy", a scenario name, ...). The
        // header row ("Buy/Sell/Repair/Identify") and any non-data rows are
        // skipped: a data row has >=4 service columns and its first field is a
        // scenario label (not "Buy"). We key rows purely by their ordinal
        // position so the lookup is stable regardless of label text.
        constexpr size_t kServiceColumns = static_cast<size_t>(MerchantServiceColumn::Count);
        if (fields.size() < 1 + kServiceColumns)
        {
            continue;
        }

        // Skip the header row (its first field is empty / "Buy" with no label).
        const std::string firstTrimmed = util::trim(fields[0]);
        if (firstTrimmed.empty())
        {
            continue;
        }

        MerchantTextRow row;
        for (size_t i = 0; i < kServiceColumns; i++)
        {
            row.services[i] = util::trim(fields[1 + i]);
        }
        rows_.push_back(std::move(row));
    }

    logger_.info(std::format("Parsed {} merchant text rows", rows_.size()));
    return true;
}

std::string_view MerchantTextParser::text(MerchantScenario scenario,
                                          MerchantServiceColumn service) const
{
    const size_t row = static_cast<size_t>(scenario);
    if (row >= rows_.size())
    {
        return {};
    }
    return rows_[row].services[static_cast<size_t>(service)];
}

} // namespace runeharbor::formats
