#include "geos.h"
#include <cstring>
#include <algorithm>
#include <ranges>
#include <bitset>

namespace d64lib::geos {

static std::string extractString(const std::vector<uint8_t>& sectorData, size_t offset, size_t maxLength) {
    if (offset >= sectorData.size()) return "";
    
    auto start = sectorData.begin() + offset;
    auto maxEnd = start + std::min(maxLength, sectorData.size() - offset);
    auto end = std::find(start, maxEnd, 0);
    
    return std::string(start, end);
}

/// <summary>
/// Check if a disk is formatted for GEOS
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <returns>true if the GEOS format string is found</returns>
bool isGeosDisk(d64& disk) {
    auto bamSector = disk.readSector(DIRECTORY_TRACK, 0);
    if (!bamSector.has_value()) return false;
    
    const auto& data = bamSector.value();
    if (data.size() < 0xBD) return false;
    
    std::string sig = extractString(data, 0xAD, 16);
    return sig.starts_with("GEOS format");
}

/// <summary>
/// Format a disk and initialize the GEOS format string
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <param name="name">name of the the disk</param>
/// <returns>true on success</returns>
bool formatGeosDisk(d64& disk, std::string_view name) {
    if (disk.TRACKS != TRACKS_35) return false;
    disk.formatDisk(name);
    
    auto bamSector = disk.readSector(DIRECTORY_TRACK, 0);
    if (!bamSector.has_value()) return false;
    
    auto data = bamSector.value();
    std::string sig = "GEOS format V1.0";
    std::copy(sig.begin(), sig.end(), data.begin() + 0xAD);
    data[0xAD + sig.length()] = 0;
    
    return disk.writeSector(DIRECTORY_TRACK, 0, data);
}

/// <summary>
/// Read the Info Block for a specific GEOS file
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <param name="filename">name of the file</param>
/// <returns>Optional InfoBlock containing GEOS metadata</returns>
std::optional<InfoBlock> readInfoBlock(d64& disk, std::string_view filename) {
    auto fileEntry = disk.findFile(filename);
    if (!fileEntry.has_value()) return std::nullopt;
    
    uint8_t infoTrack = fileEntry.value()->side.track;
    uint8_t infoSector = fileEntry.value()->side.sector;
    
    if (infoTrack == 0) return std::nullopt;
    
    auto sectorOpt = disk.readSector(infoTrack, infoSector);
    if (!sectorOpt.has_value()) return std::nullopt;
    
    const auto& sector = sectorOpt.value();
    if (sector.size() < 256) return std::nullopt;
    
    InfoBlock info;
    info.iconWidth = sector[0x02];
    info.iconHeight = 21;
    info.iconData.clear();
    int iconSizeInBytes = static_cast<int>(info.iconWidth) * info.iconHeight;
    if (iconSizeInBytes > 0 && 0x03 + iconSizeInBytes <= 0x44) {
        info.iconData.assign(sector.begin() + 0x03, sector.begin() + 0x03 + iconSizeInBytes);
    }
    
    info.dosType = sector[0x44];
    info.geosType = static_cast<FileType>(sector[0x45]);
    info.structure = static_cast<FileStructure>(sector[0x46]);
    
    info.loadAddress = sector[0x47] | (sector[0x48] << 8);
    info.endLoadAddress = sector[0x49] | (sector[0x4A] << 8);
    info.execAddress = sector[0x4B] | (sector[0x4C] << 8);
    
    info.className = extractString(sector, 0x4D, 12);
    info.version = extractString(sector, 0x59, 4);
    
    info.author = extractString(sector, 0x61, 20);
    info.description = extractString(sector, 0xA0, 96);
    
    return info;
}

/// <summary>
/// Read a Sequential GEOS file
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <param name="filename">name of the file</param>
/// <returns>Optional byte array</returns>
std::optional<std::vector<uint8_t>> readSequentialFile(d64& disk, std::string_view filename) {
    return disk.readFile(std::string(filename));
}

/// <summary>
/// Read a specific Record from a VLIR file chain
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <param name="filename">name of the file</param>
/// <param name="recordId">0-based record index (up to 127)</param>
/// <returns>Optional byte array of the record payload</returns>
std::optional<std::vector<uint8_t>> readVlirRecord(d64& disk, std::string_view filename, int recordId) {
    if (recordId < 0 || recordId > 126) return std::nullopt;
    
    auto fileEntry = disk.findFile(filename);
    if (!fileEntry.has_value()) return std::nullopt;
    
    uint8_t indexTrack = fileEntry.value()->start.track;
    uint8_t indexSector = fileEntry.value()->start.sector;
    
    if (indexTrack == 0) return std::nullopt;
    
    auto indexSectorOpt = disk.readSector(indexTrack, indexSector);
    if (!indexSectorOpt.has_value()) return std::nullopt;
    
    const auto& indexBlock = indexSectorOpt.value();
    if (indexBlock.size() < 256) return std::nullopt;
    
    int ptrOffset = 2 + (recordId * 2);
    uint8_t recordTrack = indexBlock[ptrOffset];
    uint8_t recordSector = indexBlock[ptrOffset + 1];
    
    if (recordTrack == 0x00) return std::nullopt; 
    
    std::vector<uint8_t> result;
    uint8_t currentTrack = recordTrack;
    uint8_t currentSector = recordSector;
    std::bitset<TRACKS_40 * 21> visited;
    
    while (currentTrack != 0x00) {
        if (currentTrack < 1 || currentTrack > TRACKS_40 || currentSector >= 21) break;
        size_t idx = static_cast<size_t>(currentTrack - 1) * 21 + currentSector;
        if (visited.test(idx)) break;
        visited.set(idx);
        auto sectorOpt = disk.readSector(currentTrack, currentSector);
        if (!sectorOpt.has_value()) break;
        
        const auto& sectorData = sectorOpt.value();
        if (sectorData.size() < 2) break;
        
        uint8_t nextTrack = sectorData[0];
        uint8_t nextSector = sectorData[1];
        
        if (nextTrack == 0x00) {
            if (nextSector >= 2) {
               result.insert(result.end(), sectorData.begin() + 2, sectorData.begin() + nextSector);
            }
        } else {
            result.insert(result.end(), sectorData.begin() + 2, sectorData.end());
        }
        
        currentTrack = nextTrack;
        currentSector = nextSector;
    }
    
    return result;
}

/// <summary>
/// Count the number of active records in a VLIR file
/// </summary>
/// <param name="disk">d64 disk instance</param>
/// <param name="filename">name of the file</param>
/// <returns>Count of registered records</returns>
int getVlirRecordCount(d64& disk, std::string_view filename) {
    auto fileEntry = disk.findFile(filename);
    if (!fileEntry.has_value()) return 0;
    
    uint8_t indexTrack = fileEntry.value()->start.track;
    uint8_t indexSector = fileEntry.value()->start.sector;
    
    if (indexTrack == 0) return 0;
    
    auto indexSectorOpt = disk.readSector(indexTrack, indexSector);
    if (!indexSectorOpt.has_value()) return 0;
    
    const auto& indexBlock = indexSectorOpt.value();
    
    auto indices = std::views::iota(0, 127) | std::views::reverse;
    auto it = std::ranges::find_if(indices, [&](int i) {
        int ptrOffset = 2 + (i * 2);
        return ptrOffset + 1 < indexBlock.size() && 
               (indexBlock[ptrOffset] != 0x00 || indexBlock[ptrOffset + 1] == 0xFF);
    });
    
    return (it != indices.end()) ? (*it + 1) : 0;
}

} // namespace d64lib::geos
