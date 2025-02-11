#define MONGO_LOGV2_DEFAULT_COMPONENT ::mongo::logv2::LogComponent::kDefault

#include "mongo/platform/basic.h"

#include "mongo/db/curop.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/service_context.h"

#include "mongo/s/stats/joom_top.h"

namespace mongo {

namespace {

const auto getJoomTop = ServiceContext::declareDecoration<JoomTop>();

}  // namespace

// static
JoomTop& JoomTop::get(ServiceContext* service) {
    return getJoomTop(service);
}

void JoomTop::record(OperationContext* opCtx, bool isError) {
    auto& curOp = *CurOp::get(opCtx);
    auto ns = curOp.getNS();

    if (ns[0] == '?' || curOp.getNSS().isCommand()) {
        return;
    }

    auto elapsedMicros = durationCount<Microseconds>(opCtx->getElapsedTime());
    auto cmdName = mongoCommandToCommandName(curOp.getCommand());
    auto db = curOp.getNSS().db().toString();
    auto hashedDB = UsageMap::hasher().hashed_key(db);
    auto coll = curOp.getNSS().coll().toString();
    auto hashedColl = UsageMap::hasher().hashed_key(coll);
    stdx::lock_guard<SimpleMutex> lk(_lock);

    JoomLatencyHistogram& collHist = _usage[hashedDB][hashedColl];
    _incrementHistogram(opCtx,
                        elapsedMicros,
                        &collHist,
                        cmdName,
                        isError);
}

void JoomTop::append(BSONObjBuilder& b, bool verbose) {
    stdx::lock_guard<SimpleMutex> lk(_lock);
    if (verbose) {
        _appendToUsageMapVerbose(b, _usage);
        return;
    }
    _appendToUsageMap(b, _usage);
}

/*
Adds statistics data to a response BSON object in a following format:
latencyStats: {
    <DB_NAME> : {
        <COMMAND_TYPE> {
            histogram: [{le: Long, count: Long}],
            sum: Long,
            count: Long,
            errors: Long,
            nonUserOps: Long,
        }
    }
}
*/
void JoomTop::_appendToUsageMap(BSONObjBuilder& b, const UsageMap& map) const {
    auto compactedMap = _compactUsageMap(map);
    std::vector<std::string> dbNames;
    dbNames.reserve(compactedMap.size());
    for (auto i = compactedMap.begin(); i != compactedMap.end(); ++i) {
        dbNames.push_back(i->first);
    }
    std::sort(dbNames.begin(), dbNames.end());

    BSONObjBuilder bsonStats(b.subobjStart("latencyStats"));
    for (size_t i = 0; i < dbNames.size(); ++i) {
        BSONObjBuilder bsonDB(bsonStats.subobjStart(dbNames[i]));
        const auto& dbHist = compactedMap.find(dbNames[i])->second;
        dbHist.append(&bsonDB);
        bsonDB.done();
    }
    bsonStats.done();
}

/* 
Adds statistics data to a response BSON object in a following format(extended):
latencyStats: {
    <DB_NAME> : {
        <COLLECTION_NAME> : {
            <COMMAND_NAME> {
                histogram: [{le: Long, count: Long}],
                sum: Long,
                count: Long,
                errors: Long,
                nonUserOps: Long,
            }
        }
    }
}
*/
void JoomTop::_appendToUsageMapVerbose(BSONObjBuilder& b, const UsageMap& map) const {
    std::vector<std::string> dbNames;
    dbNames.reserve(map.size());
    for (auto i = map.begin(); i != map.end(); ++i) {
        dbNames.push_back(i->first);
    }
    std::sort(dbNames.begin(), dbNames.end());

    BSONObjBuilder bsonStats(b.subobjStart("latencyStats"));
    for (size_t i = 0; i < dbNames.size(); ++i) {
        const auto& dbHist = map.find(dbNames[i])->second;
        std::vector<std::string> collNames;
        collNames.reserve(dbHist.size());
        for (auto j = dbHist.begin(); j != dbHist.end(); ++j) {
            collNames.push_back(j->first);
        }
        std::sort(collNames.begin(), collNames.end());

        BSONObjBuilder bsonDB(bsonStats.subobjStart(dbNames[i]));
        for (size_t k = 0; k < collNames.size(); ++k) {
            BSONObjBuilder bsonColl(bsonDB.subobjStart(collNames[k]));
            const auto& collHist = dbHist.find(collNames[k])->second;
            collHist.appendVerbose(&bsonColl);
            bsonColl.done();
        }
        bsonDB.done();
    }
    bsonStats.done();
}

StringMap<JoomLatencyHistogram> JoomTop::_compactUsageMap(const UsageMap& map) const {
    StringMap<JoomLatencyHistogram> result;
    for (auto i = map.begin(); i != map.end(); ++i) {
        auto hashedDB = UsageMap::hasher().hashed_key(i->first);
        auto &stats = i->second;
        for (auto j = stats.begin(); j != stats.end(); ++j) {
            result[hashedDB].add(&j->second);
        }
    }
    return result;
}

void JoomTop::_incrementHistogram(OperationContext* opCtx,
                              int64_t latency,
                              JoomLatencyHistogram* latencyHistogram,
                              CommandName cmdName,
                              bool isError) {
    // Only update histogram if operation came from a user.
    Client* client = opCtx->getClient();
    const bool isUser = client->isFromUserConnection() && !client->isInDirectClient();
    latencyHistogram->increment(cmdName, latency, isError, isUser);
}
}  // namespace mongo
