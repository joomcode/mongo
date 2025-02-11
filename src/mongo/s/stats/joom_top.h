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

    typedef StringMap<StringMap<JoomLatencyHistogram>> UsageMap;

public:
    void record(OperationContext* opCtx, bool isError);

    void append(BSONObjBuilder& b, bool verbose);

private:
    void _appendToUsageMap(BSONObjBuilder& b, const UsageMap& map) const;
    void _appendToUsageMapVerbose(BSONObjBuilder& b, const UsageMap& map) const;

    StringMap<JoomLatencyHistogram> _compactUsageMap(const UsageMap& map) const;

    void _incrementHistogram(OperationContext* opCtx,
                             int64_t latency,
                             JoomLatencyHistogram* latencyHistogram,
                             CommandName cmdName,
                             bool isError);

    mutable SimpleMutex _lock;
    UsageMap _usage;
};

inline const CommandName mongoCommandToCommandName(const Command* cmd) {
    const auto entry = StringToCommandNameMap.find(cmd->getName());
    if (entry == StringToCommandNameMap.end()) {
        return CommandName::Other;
    }

    return entry->second;
}

}  // namespace mongo
