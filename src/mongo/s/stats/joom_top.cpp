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

    auto elapsedMicros = opCtx->getElapsedTime().count();
    auto cmdName = mongoCommandToCommandName(curOp.getCommand());

    auto hashedNs = UsageMap::hasher().hashed_key(ns);
    stdx::lock_guard<SimpleMutex> lk(_lock);

    CollectionData& coll = _usage[hashedNs];
    _incrementHistogram(opCtx,
                        elapsedMicros,
                        &coll.latencyHistogram,
                        cmdName,
                        isError);
}

void JoomTop::collectionDropped(const NamespaceString& nss) {
    stdx::lock_guard<SimpleMutex> lk(_lock);
    _usage.erase(nss.ns());
}

void JoomTop::append(BSONObjBuilder& b) {
    stdx::lock_guard<SimpleMutex> lk(_lock);
    _appendToUsageMap(b, _usage);
}

/* Adds statistics data to a response BSON object. Basically, it is a map of the following format:
latencyStats: {
    <COLLECTION_NAME> : {
        <COMMAND_NAME> {
            histogram: [{le: Long, count: Long}],
            sum: Long,
            count: Long,
            errors: Long
        }
    }
}
*/
void JoomTop::_appendToUsageMap(BSONObjBuilder& b, const UsageMap& map) const {
    std::vector<std::string> names;
    for (UsageMap::const_iterator i = map.begin(); i != map.end(); ++i) {
        names.push_back(i->first);
    }
    std::sort(names.begin(), names.end());

    BSONObjBuilder bb(b.subobjStart("latencyStats"));
    for (size_t i = 0; i < names.size(); i++) {
        BSONObjBuilder bbb(bb.subobjStart(names[i]));
        const CollectionData& coll = map.find(names[i])->second;
        coll.latencyHistogram.append(&bb);
        bbb.done();
    }
    bb.done();
}

void JoomTop::_incrementHistogram(OperationContext* opCtx,
                              int64_t latency,
                              JoomLatencyHistogram* latencyHistogram,
                              CommandName cmdName,
                              bool isError) {
    // Only update histogram if operation came from a user.
    Client* client = opCtx->getClient();
    if (client->isFromUserConnection() && !client->isInDirectClient()) {
        latencyHistogram->increment(cmdName, latency, isError);
    }
}
}  // namespace mongo
