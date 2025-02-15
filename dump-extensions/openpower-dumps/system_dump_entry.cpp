#include "system_dump_entry.hpp"

#include "dump_utils.hpp"
#include "host_transport_exts.hpp"
#include "op_dump_consts.hpp"
#include "op_dump_util.hpp"
#include "system_dump_serialize.hpp"

#include <fmt/core.h>

#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace openpower
{
namespace dump
{
namespace system
{
// TODO #ibm-openbmc/issues/2859
// Revisit host transport impelementation
// This value is used to identify the dump in the transport layer to host,
constexpr auto TRANSPORT_DUMP_TYPE_IDENTIFIER = 3;
using namespace phosphor::logging;

void Entry::initiateOffload(std::string uri)
{
    log<level::INFO>(
        fmt::format(
            "System dump offload request id({}) uri({}) source dumpid({})", id,
            uri, sourceDumpId())
            .c_str());
    phosphor::dump::Entry::initiateOffload(uri);
    phosphor::dump::host::requestOffload(sourceDumpId());
}

void Entry::delete_()
{
    auto dumpId = id;
    // Skip the system dump delete if the dump is in progress
    // and in memory preserving reboot path
    if ((openpower::dump::util::isInMpReboot()) &&
        (status() == phosphor::dump::OperationStatus::InProgress))
    {
        log<level::INFO>(
            fmt::format(
                "Skip deleting system dump delete id({}) durng mp reboot",
                dumpId)
                .c_str());
        return;
    }

    auto srcDumpID = sourceDumpId();

    if ((!offloadUri().empty()) && (phosphor::dump::isHostRunning()))
    {
        log<level::ERR>(
            fmt::format("Dump offload in progress id({}) srcdumpid({})", dumpId,
                        srcDumpID)
                .c_str());
        elog<sdbusplus::xyz::openbmc_project::Common::Error::Unavailable>();
    }

    log<level::INFO>(fmt::format("System dump delete id({}) srcdumpid({})",
                                 dumpId, srcDumpID)
                         .c_str());

    auto path =
        std::filesystem::path(SYSTEM_DUMP_SERIAL_PATH) / std::to_string(dumpId);

    // Remove host system dump when host is up by using source dump id
    // which is present in system dump entry dbus object as a property.
    if ((phosphor::dump::isHostRunning()) && (srcDumpID != INVALID_SOURCE_ID))
    {
        try
        {
            phosphor::dump::host::requestDelete(srcDumpID,
                                                TRANSPORT_DUMP_TYPE_IDENTIFIER);
        }
        catch (const std::exception& e)
        {
            log<level::ERR>(fmt::format("Error deleteing dump from host id({}) "
                                        "host id({}) error({})",
                                        dumpId, srcDumpID, e.what())
                                .c_str());
            elog<sdbusplus::xyz::openbmc_project::Common::Error::Unavailable>();
        }
    }

    // Remove Dump entry D-bus object
    phosphor::dump::Entry::delete_();
    try
    {
        std::filesystem::remove_all(path);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        // Log Error message and continue
        log<level::ERR>(
            fmt::format("Failed to delete dump file({}), errormsg({})",
                        path.string().c_str(), e.what())
                .c_str());
    }

    log<level::INFO>(
        fmt::format("System dump entry with id({}) is deleted", dumpId)
            .c_str());
}

void Entry::update(uint64_t timeStamp, uint64_t dumpSize,
                   const uint32_t sourceId)
{
    elapsed(timeStamp);
    size(dumpSize);
    sourceDumpId(sourceId);
    // TODO: Handled dump failure case with
    // #bm-openbmc/2808
    status(OperationStatus::Completed);
    completedTime(timeStamp);

    // serialize as dump is successfully completed
    serialize(*this);
}

} // namespace system
} // namespace dump
} // namespace openpower
