#include "mongo/platform/basic.h"

#include "mongo/s/stats/joom_latency_histogram.h"

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/platform/bits.h"

namespace mongo {

const std::array<uint64_t, JoomLatencyHistogram::kMaxBuckets>
    JoomLatencyHistogram::kUpperBoundsMicros = {
        1000,
        5000,
        10000,
        20000,
        50000,
        100000,
        300000,
        500000,
        1000000,
        3000000,
    };

void JoomLatencyHistogram::increment(CommandName cmdName, uint64_t latency, bool isError) {
    switch (cmdName) {
        case CommandName::Insert:
            _incrementData(latency, isError, &_insert);
            break;
        case CommandName::Find:
            _incrementData(latency, isError, &_find);
            break;
        case CommandName::FindAndModify:
            _incrementData(latency, isError, &_findAndModify);
            break;
        case CommandName::Update:
            _incrementData(latency, isError, &_update);
            break;
        case CommandName::Delete:
            _incrementData(latency, isError, &_delete);
            break;
        case CommandName::Aggregate:
            _incrementData(latency, isError, &_aggregate);
            break;
        case CommandName::Distinct:
            _incrementData(latency, isError, &_distinct);
            break;
        case CommandName::Other:
            _incrementData(latency, isError, &_other);
    }
}

void JoomLatencyHistogram::append(BSONObjBuilder* builder) const {
    _append(&_insert, "insert", builder);
    _append(&_find, "find", builder);
    _append(&_findAndModify, "findAndModify", builder);
    _append(&_update, "update", builder);
    _append(&_delete, "delete", builder);
    _append(&_aggregate, "aggregate", builder);
    _append(&_distinct, "distinct", builder);
    _append(&_other, "other", builder);
}

// Binary search for a bucket corresponding to a value. Each bucket is a upper bound and 
// values must be less or equal to fit into. Values bigger than kUpperBoundsMicros[kMaxBuckets-1]
// don't fit into any bucket and count separately. 
int JoomLatencyHistogram::_getBucket(uint64_t value) {
    int l = 0;
    int r = kMaxBuckets - 1;

    if (value > kUpperBoundsMicros[r]) {
        return -1;
    }

    while (l <= r) {
        int m = l + (r - l) / 2;
        if (value <= kUpperBoundsMicros[m]) {
            r = m - 1;
        } else {
            l = m + 1;
        }
    }

    if (r < kMaxBuckets - 1) {
        return r + 1;
    }
    return r;
}

void JoomLatencyHistogram::_incrementData(uint64_t latency, bool isError, HistogramData* data) {
    if (isError) {
        data->errorCount++;
        return;
    }

    int bucket = _getBucket(latency);
    if (bucket != -1) {
        data->buckets[bucket]++;
    }
    data->sum += latency;
    data->successCount++;
}

void JoomLatencyHistogram::_append(const HistogramData* data,
                                        const char* key,
                                        BSONObjBuilder* builder) const {

    BSONObjBuilder histogramBuilder(builder->subobjStart(key));
    BSONArrayBuilder arrayBuilder(histogramBuilder.subarrayStart("histogram"));
    for (size_t i = 0; i < kMaxBuckets; i++) {
        BSONObjBuilder entryBuilder(arrayBuilder.subobjStart());
        entryBuilder.append("le", static_cast<long long>(kUpperBoundsMicros[i]));
        entryBuilder.append("count", static_cast<long long>(data->buckets[i]));
        entryBuilder.doneFast();
    }
    arrayBuilder.doneFast();

    histogramBuilder.append("sum", static_cast<long long>(data->sum));
    histogramBuilder.append("count", static_cast<long long>(data->successCount));
    histogramBuilder.append("errors", static_cast<long long>(data->errorCount));
    histogramBuilder.doneFast();
}

}  // namespace mongo
