#pragma once

#include "d64.h"
#include <string>
#include <vector>
#include <string_view>
#include <optional>
#include <cstdint>
#include <array>

namespace d64lib::geos {

enum class FileType : uint8_t {
    NonGeos = 0x00,
    Basic = 0x01,
    Assembly = 0x02,
    Data = 0x03,
    System = 0x04,
    DeskAccessory = 0x05,
    Application = 0x06,
    ApplicationData = 0x07,
    Font = 0x08,
    PrinterDriver = 0x09,
    InputDriver = 0x0A,
    DiskDriver = 0x0B,
    BootSector = 0x0C,
    Temporary = 0x0D,
    AutoExecute = 0x0E
};

enum class FileStructure : uint8_t {
    Sequential = 0x00,
    Vlir = 0x01
};

struct InfoBlock {
    uint8_t iconWidth;
    uint8_t iconHeight;
    std::vector<uint8_t> iconData;
    
    uint8_t dosType;
    FileType geosType;
    FileStructure structure;
    
    uint16_t loadAddress;
    uint16_t endLoadAddress;
    uint16_t execAddress;
    
    std::string className;
    std::string version;
    
    std::string author;
    std::string description;
};

/// <summary>
/// Check if a disk is formatted for GEOS
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <returns>true if the GEOS format string is found</returns>
bool isGeosDisk(d64& disk);

/// <summary>
/// Format a disk and initialize the GEOS format string
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <param name="name">name of the the disk</param>
/// <returns>true on success</returns>
bool formatGeosDisk(d64& disk, std::string_view name);

/// <summary>
/// Read the Info Block for a specific GEOS file
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <param name="filename">name of the file</param>
/// <returns>Optional InfoBlock containing GEOS metadata</returns>
std::optional<InfoBlock> readInfoBlock(d64& disk, std::string_view filename);

/// <summary>
/// Read a specific Record from a VLIR file chain
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <param name="filename">name of the file</param>
/// <param name="recordId">0-based record index (0-126)</param>
/// <returns>Optional byte array of the record payload</returns>
std::optional<std::vector<uint8_t>> readVlirRecord(d64& disk, std::string_view filename, int recordId);

/// <summary>
/// Read a Sequential GEOS file
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <param name="filename">name of the file</param>
/// <returns>Optional byte array</returns>
std::optional<std::vector<uint8_t>> readSequentialFile(d64& disk, std::string_view filename);

/// <summary>
/// Count the number of active records in a VLIR file
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <param name="filename">name of the file</param>
/// <returns>Count of registered records</returns>
int getVlirRecordCount(d64& disk, std::string_view filename);

} // namespace d64lib::geos
