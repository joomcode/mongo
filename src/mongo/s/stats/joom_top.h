#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>

#include "mongo/db/commands.h"
#include "mongo/db/operation_context.h"
#include "mongo/s/stats/joom_latency_histogram.h"
#include "mongo/util/concurrency/mutex.h"
#include "mongo/util/string_map.h"

namespace mongo {

class ServiceContext;

/**
 * tracks usage by collection
 */
class JoomTop {
public:
    static JoomTop& get(ServiceContext* service);

    JoomTop() = default;

    struct CollectionData {
        JoomLatencyHistogram latencyHistogram;
    };

    typedef StringMap<CollectionData> UsageMap;

public:
    void record(OperationContext* opCtx, bool isError);

    void append(BSONObjBuilder& b);

    void collectionDropped(const NamespaceString& nss);

private:
    void _appendToUsageMap(BSONObjBuilder& b, const UsageMap& map) const;

    void _incrementHistogram(OperationContext* opCtx,
                             long long latency,
                             JoomLatencyHistogram* latencyHistogram,
                             CommandName cmdName,
                             bool isError);

    mutable SimpleMutex _lock;
    UsageMap _usage;
};

inline const CommandName mongoCommandToCommandName(const Command* cmd) {
    const StringMap<CommandName> map = {
        {"insert", CommandName::Insert},
        {"find", CommandName::Find},
        {"findAndModify", CommandName::FindAndModify},
        {"update", CommandName::Update},
        {"delete", CommandName::Delete},
        {"aggregate", CommandName::Aggregate},
        {"distinct", CommandName::Distinct},
    };
    
    const auto entry = map.find(cmd->getName());
    if (entry == map.end()) {
        return CommandName::Other;
    }

    return entry->second;
}

}  // namespace mongo
