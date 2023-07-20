#include "mongo/platform/basic.h"

#include "mongo/s/stats/joom_latency_histogram.h"

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/platform/bits.h"

namespace mongo {

// latency measured in microseconds(src/mongo/s/stats/joom_top.cpp)
const std::vector<uint64_t> JoomLatencyHistogram::kUpperBoundsMicros = exponentialBucketsWithExpectedMedian(1000, 10000, 60000000, kMaxBuckets);

void JoomLatencyHistogram::increment(CommandName cmdName, uint64_t latency, bool isError, bool isUser) {
    switch (cmdName) {
        case CommandName::Insert:
            _incrementData(latency, isError, isUser, &_insert);
            break;
        case CommandName::Find:
            _incrementData(latency, isError, isUser, &_find);
            break;
        case CommandName::FindAndModify:
            _incrementData(latency, isError, isUser, &_findAndModify);
            break;
        case CommandName::Update:
            _incrementData(latency, isError, isUser, &_update);
            break;
        case CommandName::Delete:
            _incrementData(latency, isError, isUser, &_delete);
            break;
        case CommandName::Aggregate:
            _incrementData(latency, isError, isUser, &_aggregate);
            break;
        case CommandName::Distinct:
            _incrementData(latency, isError, isUser, &_distinct);
            break;
        case CommandName::Other:
            _incrementData(latency, isError, isUser, &_other);
    }
}

void JoomLatencyHistogram::add(const JoomLatencyHistogram* data) {
    this->_insert._add(&data->_insert);
    this->_find._add(&data->_find);
    this->_findAndModify._add(&data->_findAndModify);
    this->_update._add(&data->_update);
    this->_delete._add(&data->_delete);
    this->_aggregate._add(&data->_aggregate);
    this->_distinct._add(&data->_distinct);
    this->_other._add(&data->_other);
}

void JoomLatencyHistogram::append(BSONObjBuilder* builder) const {
    HistogramData _read, _write;
    _read._add(&_find);
    _read._add(&_aggregate);
    _read._add(&_distinct);
    _append(&_read, "read", builder);

    _write._add(&_insert);
    _write._add(&_findAndModify);
    _write._add(&_update);
    _write._add(&_delete);
    _append(&_write, "write", builder);

    _append(&_other, "other", builder);
}

void JoomLatencyHistogram::appendVerbose(BSONObjBuilder* builder) const {
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
    if (value > kUpperBoundsMicros[kMaxBuckets - 1]) {
        return -1;
    }

    auto it = std::lower_bound(kUpperBoundsMicros.begin(), kUpperBoundsMicros.end(), value);
    return it - kUpperBoundsMicros.begin();
}

void JoomLatencyHistogram::_incrementData(uint64_t latency, bool isError, bool isUser, HistogramData* data) {
    if (!isUser) {
        data->nonUserCount++;
        return;
    }

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
    histogramBuilder.append("nonUserOps", static_cast<long long>(data->nonUserCount));
    histogramBuilder.doneFast();
}

}  // namespace mongo
