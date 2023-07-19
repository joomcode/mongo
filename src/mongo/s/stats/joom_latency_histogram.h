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

const static std::vector<uint64_t> exponentialBuckets(uint64_t min, uint64_t max, size_t count) {
    std::vector<uint64_t> buckets;
    buckets.reserve(count);
    double step = std::exp(std::log(max/min) / static_cast<double>(count - 1));

    uint64_t value = min;
    for (size_t i = 0; i < count - 1; ++i) {
        buckets.push_back(std::round(value));
        value *= step;
    }
    buckets.push_back(max);

    return buckets;
}

const static std::vector<uint64_t> exponentialBucketsWithExpectedMedian(uint64_t min, uint64_t median, uint64_t max, size_t count) {
    size_t upperCount = count / 2;
    size_t lowerCount = count - upperCount;

    auto lowerBuckets = exponentialBuckets(min, median, lowerCount);
    auto upperBuckets = exponentialBuckets(median, max, upperCount+1);
    lowerBuckets.insert(lowerBuckets.end(), upperBuckets.begin() + 1, upperBuckets.end());

    return lowerBuckets;
}

class JoomLatencyHistogram {
public:
    static const size_t kMaxBuckets = 16;
    static const std::vector<uint64_t> kUpperBoundsMicros;
    void increment(CommandName cmdName, uint64_t latency, bool isError, bool isUser);
    void append(BSONObjBuilder* builder) const;
    void appendVerbose(BSONObjBuilder* builder) const;
    void add(const JoomLatencyHistogram* data);

private:
    struct HistogramData {
        std::array<uint64_t, kMaxBuckets> buckets{};
        uint64_t sum = 0;
        uint64_t successCount = 0;
        uint64_t errorCount = 0;
        uint64_t nonUserCount = 0;

        void _add(const HistogramData* data) {
            for (size_t i = 0; i < kMaxBuckets; i++) {
                this->buckets[i] += data->buckets[i];
            }
            this->sum += data->sum;
            this->successCount += data->successCount;
            this->errorCount += data->errorCount;
            this->nonUserCount += nonUserCount;
        }
    };

    static int _getBucket(uint64_t latency);

    void _append(const HistogramData* data,
                 const char* key,
                 BSONObjBuilder* builder) const;

    void _incrementData(uint64_t latency, bool isError, bool isUser, HistogramData* data);

    HistogramData _insert, _find, _findAndModify, _update, _delete, _aggregate, _distinct, _other;
};
}  // namespace mongo
