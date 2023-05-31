#include "mongo/platform/basic.h"

#include "mongo/base/status.h"
#include "mongo/db/commands.h"
#include "mongo/s/stats/joom_top.h"

namespace mongo {
namespace {

class JoomDiagnostics : public BasicCommand {
public:
    JoomDiagnostics() : BasicCommand("joomdiagnostics", "joomDiagnostics") {};

    AllowedOnSecondary secondaryAllowed(ServiceContext*) const override {
        return AllowedOnSecondary::kAlways;
    }

    bool requiresAuth() const override {
        return false;
    }

    bool supportsWriteConcern(const BSONObj& cmd) const override {
        return false;
    }

    virtual void addRequiredPrivileges(const std::string& dbname,
                                    const BSONObj& cmdObj,
                                    std::vector<Privilege>* out) const {}  // No auth required

    bool run(OperationContext* opCtx,
             const std::string& dbname,
             const BSONObj& cmdObj,
             BSONObjBuilder& result) override {
        JoomTop::get(opCtx->getServiceContext()).append(result);
        return true;
    }

} joomWTFCmd;

}  // namespace
}  // namespace mongo
