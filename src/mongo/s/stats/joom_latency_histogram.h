#pragma once

#include <array>

#include "mongo/db/commands.h"

namespace mongo {

class BSONObjBuilder;

enum class CommandName {
    Insert,
    Find,
    FindAndModify,
    Update,
    Delete,
    Aggregate,
    Distinct,
    Other,
};

const static StringMap<CommandName> StringToCommandNameMap = {
    {"insert", CommandName::Insert},
    {"find", CommandName::Find},
    {"findAndModify", CommandName::FindAndModify},
    {"update", CommandName::Update},
    {"delete", CommandName::Delete},
    {"aggregate", CommandName::Aggregate},
    {"distinct", CommandName::Distinct},
};

class JoomLatencyHistogram {
public:
    static const int kMaxBuckets = 10;
    static const std::array<uint64_t, kMaxBuckets> kUpperBoundsMicros;
    void increment(CommandName cmdName, uint64_t latency, bool isError, bool isUser);
    void append(BSONObjBuilder* builder) const;

private:
    struct HistogramData {
        std::array<uint64_t, kMaxBuckets> buckets{};
        uint64_t sum = 0;
        uint64_t successCount = 0;
        uint64_t errorCount = 0;
        uint64_t nonUserCount = 0;
    };

    static int _getBucket(uint64_t latency);

    void _append(const HistogramData* data,
                 const char* key,
                 BSONObjBuilder* builder) const;

    void _incrementData(uint64_t latency, bool isError, bool isUser, HistogramData* data);

    HistogramData _insert, _find, _findAndModify, _update, _delete, _aggregate, _distinct, _other;
};
}  // namespace mongo
