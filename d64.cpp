// Written by Paul Baxter

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <iomanip>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <map>
#include <sstream>
#include <bitset>

#include "d64.h"

#pragma warning(disable:4267 28020 6011)

/// <summary>
/// constructor with no parameters
/// create a blank disk
/// </summary>
d64::d64()
{
    init_disk();
}

/// <summary>
/// constructor with disktype
/// </summary>
/// <param name="type">disktype</param>
d64::d64(diskType type)
{
    disktype = type;
    init_disk();
}

/// <summary>
/// constructor with a file name
/// load the disk from an existing d64 file
/// </summary>
/// <param name="name">name of file to load</param>
d64::d64(std::string name)
{
    // load the disk
    if (!load(name)) {
        throw std::invalid_argument("Unable to load disk");
    }
}

/// <summary>
/// Initialize 35 or 40 track
/// </summary>
void d64::init_disk()
{
    int sz = 0;
    switch (disktype) {
        case diskType::thirty_five_track:
            TRACKS = TRACKS_35;
            sz = D64_DISK35_SZ;
            break;
        case diskType::forty_track:
            TRACKS = TRACKS_40;
            sz = D64_DISK40_SZ;
            break;
        default:
            throw std::runtime_error("Invalid Disk type");
    }
    data.resize(sz, 0x01);
    std::fill_n(lastSectorUsed.begin(), TRACKS_40, 1);
    formatDisk("NEW DISK");
}

// NOTE: track starts at 1. returns offset int datafor track and sector
int d64::calcOffset(int track, int sector) const
{
    if (!isValidTrackSector(track, sector)) {
        throw std::runtime_error("Invalid Track and Sector TRACK:" + std::to_string(track) + " SECTOR:" + std::to_string(sector));
    }
    return TRACK_OFFSETS[track - 1] + sector * SECTOR_SIZE;
}

/// <summary>
/// Initialize BAM
/// and name the disk
/// </summary>
/// <param name="name">new name for disk</param>
void d64::initBAM(std::string_view name)
{
    // set the BAM pointer
    initBAMPtr();

    // initialize the BAM fields
    // bamPtr must already be set!
    initializeBAMFields(name);

    // Mark all sectors free
    for (auto t = 0; t < TRACKS; ++t) {
        auto bam = bamtrack(t);
        bam->free = SECTORS_PER_TRACK[t];
        bam->clear();
        for (auto b = 0; b < SECTORS_PER_TRACK[t]; b++) {
            bam->set(b);
        }
    }

    // Initialize the directory structure
    auto index = calcOffset(DIRECTORY_TRACK, DIRECTORY_SECTOR);
    std::fill_n(data.begin() + index, SECTOR_SIZE, 0);

    // mark as the last directory sector
    data[index + 1] = 0xFF;

    // allocate the BAM sector
    allocateSector(DIRECTORY_TRACK, BAM_SECTOR);

    // allocate the 1st directory sector
    allocateSector(DIRECTORY_TRACK, DIRECTORY_SECTOR);
}

/// <summary>
/// Rename the disk
/// </summary>
/// <param name="name">new name for disk</param>
bool d64::rename_disk(std::string_view name) const
{
    auto len = std::min(name.size(), static_cast<size_t>(DISK_NAME_SZ));
    std::copy_n(name.begin(), len, diskBamPtr->diskName);
    std::fill(diskBamPtr->diskName + len, diskBamPtr->diskName + DISK_NAME_SZ, static_cast<char>(A0_VALUE));
    return true;
}

/// <summary>
/// Format the disk and set new name
/// </summary>
/// <param name="name">new name for disk</param>
void d64::formatDisk(std::string_view name)
{
    // format with 1's
    std::fill(data.begin(), data.end(), 0x01);

    // intialize BAM
    initBAM(name);
}

/// <summary>
/// write an entire sector
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector number</param>
/// <param name="bytes">arrray of SECTOR_SIZE bytes</param>
/// <returns>true on success</returns>
bool d64::writeSector(int track, int sector, std::vector<uint8_t> bytes)
{
    try {
        if (!isValidTrackSector(track, sector) || bytes.size() != SECTOR_SIZE) {
            throw std::invalid_argument("Invalid track, sector, or byte size");
        }
        return writeData(track, sector, bytes, 0);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        throw; // Rethrow the exception
    }
    return false;
}

/// <summary>
/// Write a byte to a sector
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector number</param>
/// <param name="offset">byte of sector</param>
/// <param name="value">value to write</param>
/// <returns>true on success</returns>
bool d64::writeByte(int track, int sector, int byteoffset, uint8_t value)
{
    if (!isValidTrackSector(track, sector) || byteoffset < 0 || byteoffset >= SECTOR_SIZE) return false;
    return writeData(track, sector, { value }, byteoffset);
}

/// <summary>
/// Read a byte from a sector
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector number</param>
/// <param name="offset">byte of sector</param>
/// <returns>optional data read</returns>
std::optional<uint8_t> d64::readByte(int track, int sector, int byteoffset)
{
    if (!isValidTrackSector(track, sector) || byteoffset < 0 || byteoffset >= SECTOR_SIZE) return std::nullopt;
    auto offset = calcOffset(track, sector) + byteoffset;
    if (offset >= 0 && offset < static_cast<int>(data.size())) {
        return data[offset];
    }
    return std::nullopt;
}

/// <summary>
/// Read a sector
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector number</param>
/// <returns>optional vector of data read</returns>
std::optional<std::vector<uint8_t>> d64::readSector(int track, int sector)
{
    if (!isValidTrackSector(track, sector)) return std::nullopt;
    std::vector<uint8_t> bytes(SECTOR_SIZE);
    auto index = calcOffset(track, sector);
    if (index >= 0 && index + SECTOR_SIZE <= static_cast<int>(data.size())) {
        std::copy_n(data.begin() + index, SECTOR_SIZE, bytes.begin());
        return bytes;
    }
    return std::nullopt;
}

/// <summary>
/// Find an empty slot in the directory
/// This will create a new directory sector if needed
/// </summary>
/// <returns>optional Directory_EntryPtr to free slot</returns>
std::optional<directoryEntryPtr> d64::findEmptyDirectorySlot()
{
    auto dir_track = DIRECTORY_TRACK;
    auto dir_sector = DIRECTORY_SECTOR;

    while (dir_track != 0) {
        directorySectorPtr dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
        for (auto& fileEntry : dirSectorPtr->fileEntry) {
            if (!fileEntry.file_type.closed) {
                return &fileEntry;
            }
        }
        dir_track = dirSectorPtr->next.track;
        dir_sector = dirSectorPtr->next.sector;

        if (dir_track == 0 || dir_track > TRACKS || dir_sector < 0 || dir_sector > SECTORS_PER_TRACK[dir_track - 1]) {
            if (!allocateNewDirectorySector(dir_track, dir_sector, dirSectorPtr)) {
                throw std::runtime_error("Disk full. Unable to find directory slot");
            }
        }
    }
    return std::nullopt;
}

bool d64::allocateNewDirectorySector(int& dir_track, int& dir_sector, directorySectorPtr& dirSectorPtr)
{
    if (!findAndAllocateFreeSector(dir_track, dir_sector, true)) {
        return false;
    }
    dirSectorPtr->next.track = dir_track;
    dirSectorPtr->next.sector = dir_sector;
    dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
    memset(dirSectorPtr, 0, SECTOR_SIZE);
    dirSectorPtr->next.track = 0;
    dirSectorPtr->next.sector = 0xFF;
    return true;
}

/// <summary>
/// Allocate a side sector for .REL file
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector numbe</param>
/// <param name="side">pointer to SideSector</param>
/// <returns>true on success</returns>
bool d64::allocateSideSector(int& track, int& sector, sideSectorPtr& side)
{
    if (!findAndAllocateFreeSector(track, sector, false)) return false;
    side = getSideSectorPtr(track, sector);
    memset(side, 0, SECTOR_SIZE);
    return true;
}

/// <summary>
/// Allocate a data sector for .REL file
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector number</param>
/// <param name="sectorPtr">pointer to Sector</param>
/// <returns>true on success</returns>
bool d64::allocateDataSector(int& track, int& sector, sectorPtr& sectorPtr)
{
    if (!findAndAllocateFreeSector(track, sector, false)) return false;
    sectorPtr = getSectorPtr(track, sector);
    sectorPtr->next.track = 0;
    sectorPtr->next.sector = 0;
    return true;
}

/// <summary>
/// Write data to a sector
/// </summary>
/// <param name="sectorPtr">pointer to sector to write to</param>
/// <param name="fileData">data to write</param>
/// <param name="offset">offset of data</param>
/// <param name="bytesLeft">number of bytes left to write</param>
void d64::writeDataToSector(sectorPtr sectorPtr, const std::vector<uint8_t>& fileData, int& offset, int& bytesLeft)
{
    if (!sectorPtr) {
        throw std::invalid_argument("Invalid null sector pointer");
    }
    
    if (offset < 0 || static_cast<size_t>(offset) >= fileData.size()) {
        throw std::out_of_range("File data offset out of range: " + std::to_string(offset));
    }
    
    if (bytesLeft < 0) {
        throw std::invalid_argument("Invalid negative bytes left value");
    }
    
    const size_t availableBytes = fileData.size() - static_cast<size_t>(offset);
    const size_t bufferSize = sectorPtr->data.size();
    const size_t len = std::min({bufferSize, static_cast<size_t>(bytesLeft), availableBytes});
    
    if (len > bufferSize) {
        throw std::out_of_range("Data length exceeds sector buffer size");
    }
    
    if (static_cast<size_t>(offset) > SIZE_MAX - len) {
        throw std::overflow_error("Offset plus length would overflow");
    }
    
    if (len > 0) {
        if (static_cast<size_t>(offset) + len > fileData.size()) {
            throw std::out_of_range("Read operation would exceed file data bounds");
        }
        std::copy_n(fileData.begin() + offset, len, sectorPtr->data.begin());
    }
    
    if (len < bufferSize) {
        std::fill_n(sectorPtr->data.begin() + len, bufferSize - len, 0);
    }
    
    if (len > static_cast<size_t>(bytesLeft)) {
        throw std::invalid_argument("Length exceeds remaining bytes");
    }
    
    bytesLeft -= static_cast<int>(len);
    offset += static_cast<int>(len);
    
    if (bytesLeft == 0) {
        if (len > 254) {
            throw std::out_of_range("Sector length exceeds maximum value");
        }
        sectorPtr->next.track = 0;
        sectorPtr->next.sector = static_cast<uint8_t>(len + 1);
    }
}


/// <summary>
/// Add a file to the disk
/// </summary>
/// <param name="filename">file to load</param>
/// <param name="type">file type</param>
/// <param name="fileData">data to the file</param>
/// <returns>true if successful</returns>
/// <summary>
bool d64::addFile(std::string_view filename, c64FileType type, const std::vector<uint8_t>& fileData, int recordSize)
{
    // Validate inputs
    if (filename.empty() || fileData.empty()) {
        throw std::runtime_error("Error: Filename or file data cannot be empty");
    }

    // Find and allocate the first sector for the file
    int start_track, start_sector;
    if (!findAndAllocateFirstSector(start_track, start_sector)) {
        return false;
    }

    // Write file data to sectors
    auto allocatedSectors = writeFileDataToSectors(start_track, start_sector, fileData);

    // Create a directory entry for the file
    if (!createDirectoryEntry(filename, type, start_track, start_sector, allocatedSectors, recordSize)) {
        return false;
    }

    return true;
}

/// <summary>
/// Find and allocate the first sector for a file
/// </summary>
/// <param name="start_track">first track number</param>
/// <param name="start_sector">first sector number</param>
/// <returns>true on success</returns>
bool d64::findAndAllocateFirstSector(int& start_track, int& start_sector)
{
    if (!findAndAllocateFreeSector(start_track, start_sector, false)) {
        throw std::runtime_error("Disk full. Unable to find free sector");
    }
    return true;
}

/// <summary>
/// Write file data to disk
/// first block must be already allocated
/// </summary>
/// <param name="start_track">starting track</param>
/// <param name="start_sector">starting sector></param>
/// <param name="fileData">data to write</param>
/// <returns>vector of allocated sectors</returns>
std::vector<trackSector> d64::writeFileDataToSectors(int start_track, int start_sector, const std::vector<uint8_t>& fileData)
{
    std::vector<trackSector> allocatedSectors;
    int next_track = start_track;
    int next_sector = start_sector;
    int offset = 0;
    int bytesLeft = static_cast<int>(fileData.size());

    while (offset < static_cast<int>(fileData.size())) {
        int track = next_track;
        int sector = next_sector;

        if (fileData.size() - offset > (SECTOR_SIZE - 2)) {
            if (!findAndAllocateFreeSector(next_track, next_sector, false)) {
                throw std::runtime_error("Disk full. Unable to add file data");
            }
        }
        else {
            next_track = 0;
            next_sector = bytesLeft + 1;
        }

        auto sectorPtr = getSectorPtr(track, sector);
        sectorPtr->next.track = static_cast<uint8_t>(next_track);
        sectorPtr->next.sector = static_cast<uint8_t>(next_sector);

        writeDataToSector(sectorPtr, fileData, offset, bytesLeft);
        allocatedSectors.emplace_back(track, sector);
    }

    return allocatedSectors;
}

/// <summary>
/// Create a side sector list
/// </summary>
/// <param name="allocatedSectors">list of file sectors</param>
/// <param name="record_size">size of each record</param>
/// <returns>list of side sectors</returns>
std::optional<std::vector<sideSectorPtr>> d64::createSideSectors(const std::vector<trackSector>& allocatedSectors, uint8_t record_size)
{
    int ssecTrack, ssecSector;
    sideSectorPtr sideSector{};

    std::vector<sideSectorPtr> sideSectorList;
    int block = 0;
    int sideCount = 0;
    int chainCount = 0;

    if (!allocateSideSector(ssecTrack, ssecSector, sideSector)) return std::nullopt;

    sideSectorList.push_back(sideSector);
    sideSector->recordsize = record_size;
    sideSector->next = { 0, 16 };
    sideSector->block = block++;
    sideSectorPtr first = sideSector;
    first->sideSectors[sideCount++] = { ssecTrack, ssecSector };

    for (const auto& ts : allocatedSectors) {
        if (chainCount < SIDE_SECTOR_CHAIN_SZ) {
            sideSector->chain[chainCount++] = ts;
            sideSector->next.sector += 2;
        }
        else {
            if (sideSectorList.size() >= SIDE_SECTOR_ENTRY_SIZE) {
                throw std::runtime_error("Exceeded maximum number of side sectors (6)");
            }
            if (!allocateSideSector(ssecTrack, ssecSector, sideSector)) return std::nullopt;
            first->sideSectors[sideCount++] = { ssecTrack, ssecSector };
            sideSectorList.back()->next = { ssecTrack, ssecSector };
            sideSector->block = block++;
            sideSector->recordsize = record_size;
            chainCount = 0;
            sideSector->chain[chainCount++] = ts;
            sideSector->next = { 0, 16 + 2 };
        }
    }

    for (auto& sideSectors : sideSectorList) {
        std::copy_n(first->sideSectors, sideCount, sideSectors->sideSectors);
    }
    return sideSectorList;
}

/// <summary>
/// Create a directory entry
/// </summary>
/// <param name="filename">name of file</param>
/// <param name="type">type of file</param>
/// <param name="start_track">first track</param>
/// <param name="start_sector">first sector</param>
/// <param name="allocated_sectors">number of sectors</param>
/// <returns>true on success</returns>
bool d64::createDirectoryEntry(std::string_view filename, c64FileType type, int start_track, int start_sector, const std::vector<trackSector>& allocatedSectors, uint8_t record_size)
{
    auto fileEntry = findEmptyDirectorySlot();
    if (!fileEntry.has_value()) {
        return false;
    }

    fileEntry.value()->file_type = type;
    fileEntry.value()->start.track = start_track;
    fileEntry.value()->start.sector = start_sector;

    auto len = std::min(filename.size(), static_cast<size_t>(FILE_NAME_SZ));
    std::copy_n(filename.begin(), len, fileEntry.value()->fileName);
    std::fill(fileEntry.value()->fileName + len, fileEntry.value()->fileName + FILE_NAME_SZ, static_cast<char>(A0_VALUE));

    // create side sectors for .REL files
    if (type.type == d64FileTypes::REL) {
        auto sideSectorList = createSideSectors(allocatedSectors, record_size);
        if (!sideSectorList.has_value() || sideSectorList.value().size() < 1) {
            throw std::runtime_error("Error: Unable to create side sector list");
        }
        fileEntry.value()->recordLength = record_size;
        fileEntry.value()->side.track = sideSectorList.value()[0]->sideSectors[0].track;
        fileEntry.value()->side.sector = sideSectorList.value()[0]->sideSectors[0].sector;
    }
    else {
        fileEntry.value()->recordLength = 0;
        fileEntry.value()->side.track = 0;
        fileEntry.value()->side.sector = 0;
    }

    fileEntry.value()->replace.track = fileEntry.value()->start.track;
    fileEntry.value()->replace.sector = fileEntry.value()->start.sector;
    fileEntry.value()->fileSize[0] = allocatedSectors.size() & 0xFF;
    fileEntry.value()->fileSize[1] = (allocatedSectors.size() & 0xFF00) >> 8;

    return true;
}

/// <summary>
/// verify the BAM integrity
/// </summary>
/// <param name="fix">true to auto fix</param>
/// <param name="logFile">logfile name or "" for std::cerr</param>
/// <returns>true on success</returns>
bool d64::verifyBAMIntegrity(bool fix, const std::string& logFile)
{
    // Open log file if specified
    std::ofstream logStream;
    std::ostream* logOutput = &std::cerr;

    if (!logFile.empty()) {
        logStream.open(logFile, std::ios::out);
        if (logStream.is_open()) {
            logOutput = &logStream;
        }
        else {
            std::cerr << "WARNING: Failed to open log file. Logging to std::cerr instead.\n";
        }
    }

    // Temp map to count sector usage
    std::array<std::array<bool, 21>, TRACKS_40> sectorUsage = {}; // Max sectors per track

    // **Step 1: Mark BAM itself as used**
    sectorUsage[DIRECTORY_TRACK - 1][BAM_SECTOR] = true;

    // **Step 2: Scan directory for used sectors**
    auto dir_track = DIRECTORY_TRACK;
    auto dir_sector = DIRECTORY_SECTOR;
    directorySectorPtr dirSectorPtr;

    while (dir_track != 0) {
        dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

        // Mark directory sector as used
        sectorUsage[dir_track - 1][dir_sector] = true;

        for (auto& entry : dirSectorPtr->fileEntry) {

            if ((entry.file_type.closed) == 0) continue; // Skip deleted files

            int track = entry.start.track;
            int sector = entry.start.sector;
            sectorUsage[track - 1][sector] = true;

            if (entry.file_type.type == d64FileTypes::REL) {
                // get the first location of a side sector
                trackSector sidePosition = entry.side;

                // load the side sector 
                sideSectorPtr side = getSideSectorPtr(sidePosition.track, sidePosition.sector);
                for (auto side_sectors : side->sideSectors) {
                    if (side_sectors.track == 0)
                        break;

                    side = getSideSectorPtr(side_sectors.track, side_sectors.sector);
                    sectorUsage[side_sectors.track - 1][side_sectors.sector] = true;

                    for (auto chainEntry : side->chain) {
                        if (chainEntry.track == 0) {
                            break;
                        }
                        sectorUsage[chainEntry.track - 1][chainEntry.sector] = true;
                    }
                }
            }
            else {
                while (track != 0) {
                    sectorUsage[track - 1][sector] = true;
                    auto next = getTrackSectorPtr(track, sector);

                    track = next->track;
                    sector = next->sector;
                }
            }
        }

        dir_track = dirSectorPtr->next.track;
        dir_sector = dirSectorPtr->next.sector;
    }

    // **Step 3: Compare BAM against actual usage**
    auto errorsFound = false;

    for (auto track = 1; track <= TRACKS; ++track) {
        auto correctFreeCount = 0;

        for (auto sector = 0; sector < SECTORS_PER_TRACK[track - 1]; ++sector) {
            auto isFreeInBAM = bamtrack(track - 1)->test(sector);
            auto isUsedInDirectory = sectorUsage[track - 1][sector];

            // Error: Sector incorrectly marked as used
            if (!isUsedInDirectory && !isFreeInBAM) {
                *logOutput << "ERROR: Sector " << sector << " on Track " << track
                    << " is incorrectly marked as used in BAM.\n";
                errorsFound = true;

                if (fix) {
                    *logOutput << "FIXING: Freeing sector " << sector << " on Track " << track << ".\n";
                    bamtrack(track - 1)->set(sector);
                }
            }

            // Error: Sector incorrectly marked as free
            else if (isUsedInDirectory && isFreeInBAM) {
                *logOutput << "ERROR: Sector " << sector << " on Track " << track
                    << " is incorrectly marked as free in BAM.\n";
                errorsFound = true;

                if (fix) {
                    *logOutput << "FIXING: Marking sector " << sector << " on Track " << track << " as used.\n";
                    bamtrack(track - 1)->reset(sector);
                }
            }

            if (!isUsedInDirectory) {
                correctFreeCount++;
            }
        }

        // Error: Incorrect free sector count
        int free = bamtrack(track - 1)->free;

        if (free != correctFreeCount) {
            *logOutput << "WARNING: BAM free sector count mismatch on Track " << track
                << " (BAM: " << free
                << ", Expected: " << correctFreeCount << ")\n";
            errorsFound = true;

            if (fix) {
                *logOutput << "FIXING: Correcting free sector count for Track " << track << ".\n";
                bamtrack(track - 1)->free = correctFreeCount;
            }
        }
    }

    // Close log file if used
    if (logStream.is_open()) {
        logStream.close();
    }

    return !errorsFound; // Return true if BAM is valid
}

/// <summary>
/// Reorder the files on the disk
/// </summary>
/// <param name="fileOrder">order of files</param>
/// <returns>true on success</returns>
bool d64::reorderDirectory(const std::vector<std::string>& fileOrder)
{
    std::vector<directoryEntry> files = directory();
    std::vector<directoryEntry> reorderedFiles;

    // **Step 1: Add files in the specified order**
    for (const auto& filename : fileOrder) {
        auto it = std::find_if(files.begin(), files.end(), [&](const directoryEntry& entry)
            {
                return Trim(entry.fileName) == filename;
            });

        if (it != files.end()) {
            reorderedFiles.push_back(*it);
            files.erase(it);
        }
    }

    // **Step 2: Append any remaining files that were not explicitly ordered**
    reorderedFiles.insert(reorderedFiles.end(), files.begin(), files.end());

    // **Step 3: Avoid unnecessary writes**
    if (directory() == reorderedFiles)
        return false; // No change needed

    // **Step 4: Write new order to directory**
    return reorderDirectory(reorderedFiles);
}

/// <summary>
/// compact directory
/// </summary>
/// <returns>true on success</returns>
bool d64::compactDirectory()
{
    std::vector<directoryEntry> files;

    int dir_track = DIRECTORY_TRACK;
    int dir_sector = DIRECTORY_SECTOR;
    directorySectorPtr dirSectorPtr;

    // **Step 1: Collect all valid directory entries**
    while (dir_track != 0) {
        dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

        for (auto &entry : dirSectorPtr->fileEntry) {
            if ((entry.file_type.closed) == 0)
                continue; // Skip deleted files

            files.push_back(entry);
        }

        dir_track = dirSectorPtr->next.track;
        dir_sector = dirSectorPtr->next.sector;
    }

    if (files.empty()) return false; // No valid files

    // **Step 2: Rewrite the directory with compacted entries**
    dir_track = DIRECTORY_TRACK;
    dir_sector = DIRECTORY_SECTOR;
    dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

    size_t index = 0;
    bool freedSector = false;

    while (dir_track != 0) {
        std::fill_n(reinterpret_cast<uint8_t*>(dirSectorPtr), SECTOR_SIZE, 0); // Clear sector

        for (auto i = 0; i < FILES_PER_SECTOR && index < files.size(); ++i, ++index) {
            dirSectorPtr->fileEntry[i] = files[index];
        }

        // **Step 3: If no more files, free remaining sectors**
        if (index >= files.size()) {
            // Mark remaining directory sectors as free in BAM
            while (dir_track != 0) {
                int next_track = dirSectorPtr->next.track;
                int next_sector = dirSectorPtr->next.sector;

                // never mark track 18 as free
                if (dir_track != DIRECTORY_TRACK || dir_sector != DIRECTORY_SECTOR) {

                    // Free the sector in BAM
                    freeSector(dir_track, dir_sector);
                    freedSector = true;
                }

                if (next_track == 0) break; // No more directory sectors

                dir_track = next_track;
                dir_sector = next_sector;
                dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
            }
            break;
        }

        dir_track = dirSectorPtr->next.track;
        dir_sector = dirSectorPtr->next.sector;
        if (dir_track != 0) dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
    }

    if (freedSector) {
        std::cerr << "FIXED: Freed unused directory sectors and updated BAM.\n";
    }

    return true;
}

/// <summary>
/// find a file on the disk
/// </summary>
/// <param name="filename">file to find</param>
/// <returns>optional pointer to the fiels directory entry</returns>
std::optional<directoryEntryPtr> d64::findFile(std::string_view filename)
{
    try {
        auto dir_track = DIRECTORY_TRACK;
        auto dir_sector = DIRECTORY_SECTOR;

        while (dir_track != 0) {
            auto dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
            for (auto& fileEntry : dirSectorPtr->fileEntry) {
                if (fileEntry.file_type.closed == 0) {
                    continue;
                }
                std::string entryName(fileEntry.fileName, FILE_NAME_SZ);
                entryName.erase(std::find_if(entryName.begin(), entryName.end(), [](char c) { return c == static_cast<char>(A0_VALUE); }), entryName.end());
                if (entryName == filename) {
                    return &fileEntry;
                }
            }
            dir_track = dirSectorPtr->next.track;
            dir_sector = dirSectorPtr->next.sector;
        }
    }
    catch (const std::out_of_range& e) {
        std::cerr << "Out of range error: " << e.what() << std::endl;
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return std::nullopt;
}

/// <summary>
/// remove a file from the disk
/// </summary>
/// <param name="filename">file to remove</param>
/// <returns>true if successful</returns>
bool d64::removeFile(std::string_view filename)
{
    try {
        auto fileEntry = findFile(filename);
        if (!fileEntry.has_value()) {
            throw std::runtime_error("File not found: " + std::string(filename));
        }
        int track = fileEntry.value()->start.track;
        int sector = fileEntry.value()->start.sector;

        while (track != 0) {
            auto sectorPtr = getTrackSectorPtr(track, sector);
            auto next_track = sectorPtr->track;
            auto next_sector = sectorPtr->sector;
            freeSector(track, sector);
            track = next_track;
            sector = next_sector;
        }

        memset(fileEntry.value(), 0, sizeof(directoryEntry));
        return true;
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return false;
}

/// <summary>
/// Rename a file
/// </summary>
/// <param name="oldfilename">old file name</param>
/// <param name="newfilename">new file name</param>
/// <returns>true if successful</returns>
bool d64::renameFile(std::string_view oldfilename, std::string_view newfilename)
{
    auto fileEntry = findFile(oldfilename);
    if (!fileEntry.has_value()) {
        throw std::runtime_error("File not found: " + std::string(oldfilename));
    }
    auto len = std::min(newfilename.size(), static_cast<size_t>(FILE_NAME_SZ));
    std::copy_n(newfilename.begin(), len, fileEntry.value()->fileName);
    std::fill(fileEntry.value()->fileName + len, fileEntry.value()->fileName + FILE_NAME_SZ, static_cast<char>(A0_VALUE));
    return true;
}

/// <summary>
/// extract a file from the disk
/// </summary>
/// <param name="filename">file to extrack</param>
/// <returns>true if successful</returns>
bool d64::extractFile(std::string filename)
{
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value()) {
        throw std::runtime_error("File not found: " + std::string(filename));
    }

    static const std::map<d64FileTypes, std::string> extMap = {
        { d64FileTypes::PRG, ".prg" },
        { d64FileTypes::SEQ, ".seq" },
        { d64FileTypes::USR, ".usr" },
        { d64FileTypes::REL, ".rel" },
    };
    auto it = extMap.find(fileEntry.value()->file_type.type);
    if (it == extMap.end()) {
        throw std::runtime_error("Unknown file type: " + std::to_string(static_cast<uint8_t>(fileEntry.value()->file_type)));
    }

    auto fileData = readFile(filename);
    if (!fileData.has_value()) {
        throw std::runtime_error("Unable to read file " + filename);
    }

    auto ext = extMap.at(fileEntry.value()->file_type.type);
    try {
        std::ofstream outFile((filename + ext).c_str(), std::ios::binary);
        if (!outFile.is_open()) {
            throw std::runtime_error("Failed to open output file: " + filename + ext);
        }
        
        outFile.write(reinterpret_cast<char*>(fileData->data()), fileData->size());
        if (outFile.fail()) {
            throw std::runtime_error("Failed to write to file: " + filename + ext);
        }
        
        outFile.close();
        if (outFile.fail()) {
            throw std::runtime_error("Failed to close file: " + filename + ext);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error extracting file: " << e.what() << std::endl;
        return false;
    }

    return true;
}

/// <summary>
/// get file data from the disk
/// </summary>
/// <param name="filename">file to read</param>
/// <returns>true if successful</returns>
std::optional<std::vector<uint8_t>> d64::readFile(std::string filename)
{
    // find the file
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value()) {
        throw std::runtime_error("File not found: " + filename);
    }

    // the file data will be stored here
    std::vector<uint8_t> fileData;

    // get the files start track and sector
    int track = fileEntry.value()->start.track;
    int sector = fileEntry.value()->start.sector;

    // track 0 signifies end
    while (track != 0) {

        // get the next track and sector of file
        auto sectorPtr = getSectorPtr(track, sector);

        // if the track is not zero then write the whole block
        int bytes = sectorPtr->next.track != 0 ? sizeof(sectorPtr->data) : sectorPtr->next.sector - 1;
        // append the data at the end 
        fileData.insert(fileData.end(), sectorPtr->data.begin(), sectorPtr->data.begin() + bytes);

        // set current track and sector
        track = sectorPtr->next.track;
        sector = sectorPtr->next.sector;
    }

    // exit
    return fileData;
}

/// <summary>
/// Get the name of the disk
/// </summary>
/// <returns>Name of the disk</returns>
std::string d64::diskname()
{
    std::string name;
    for (auto& ch : diskBamPtr->diskName) {
        if (ch == static_cast<char>(0xA0)) return name;
        name += ch;
    }
    return name;
}

/// <summary>
/// save image to disk
/// </summary>
/// <param name="filename">name of file</param>
/// <returns>true if successful</returns>
bool d64::save(std::string filename)
{
    // open the file
    std::ofstream outFile(filename.c_str(), std::ios::binary);
    if (!outFile) {
        throw std::runtime_error("Error: Could not open file for writing");
    }
    // write all the data
    outFile.write(reinterpret_cast<char*>(data.data()), data.size());
    outFile.close();
    return true;
}

/// <summary>
/// load a disk image
/// </summary>
/// <param name="filename">name of .d64 file to load</param>
/// <returns>true if sucessful</returns>
bool d64::load(std::string filename)
{
    try {
        // open the file
        std::ifstream inFile(filename, std::ios::binary);
        if (!inFile) {
            throw std::ios_base::failure("Error: Could not open disk file " + filename + " for reading");
        }
        inFile.seekg(0, std::ios::end);
        auto pos = inFile.tellg();

        inFile.seekg(0, std::ios::beg);
        if (pos != D64_DISK35_SZ && pos != D64_DISK40_SZ) {
            throw std::invalid_argument("Invalid disk size");
        }
        disktype = (pos == D64_DISK35_SZ) ? diskType::thirty_five_track : diskType::forty_track;

        // allocate the disk
        init_disk();

        // read the data
        inFile.read(reinterpret_cast<char*>(data.data()), data.size());

        // close the file
        inFile.close();

        // validate the disk
        if (!validateD64()) {
            formatDisk("NEW DISK");
        }

        return true;
    }
    catch (const std::ios_base::failure& e) {
        std::cerr << "I/O error: " << e.what() << std::endl;
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return false;
}

/// <summary>
/// Free a sector
/// </summary>
/// <param name="track">Track number</param>
/// <param name="sector">sector number</param>
/// <returns>true if successful. If the sector is already free false</returns>
bool d64::freeSector(const int& track, const int& sector)
{
    // validate track sector number
    if (!isValidTrackSector(track, sector)) {
        throw std::runtime_error("Invalid Tack and Sector TRACK:" + std::to_string(track) + " SECTOR:" + std::to_string(sector));
    }
    if (track == DIRECTORY_TRACK && sector == DIRECTORY_SECTOR) {
        std::cerr << "Warning: Attempt to free directory sector ignored (Track 18, Sector 1)\n";
        return false;
    }
    if (track == DIRECTORY_TRACK && sector == BAM_SECTOR) {
        std::cerr << "Warning: Attempt to free directory sector ignored (Track 18, Sector 0)\n";
        return false;
    }

    // calculate byte of BAM entry for track
    // its stored as a bitmap of 3 bytes. 1 if free and 0 if allocated    
    if (bamtrack(track - 1)->test(sector))
        return false;
    
    bamtrack(track - 1)->set(sector);   // mark track sector as free 
    bamtrack(track - 1)->free++;            // increment free

    return true;
}

/// <summary>
/// Check the validity of the track and sector number
/// </summary>
/// <param name="track">track number to check</param>
/// <param name="sector">sector number to check</param>
/// <returns>true if valid</returns>
bool d64::isValidTrackSector(int track, int sector) const
{
    return track >= 1 && track <= TRACKS && sector >= 0 && sector < SECTORS_PER_TRACK[track - 1];
}

/// <summary>
/// allocate a sector
/// </summary>
/// <param name="track">Track number</param>
/// <param name="sector">sector number</param>
/// <returns>true if successful. If the sector is already allocated return false</returns>
bool d64::allocateSector(const int& track, const int& sector)
{
    // validate track sector number
    if (!isValidTrackSector(track, sector)) {
        throw std::runtime_error("Invalid Tack and Sector TRACK:" + std::to_string(track) + " SECTOR:" + std::to_string(sector));
    }

    if (!bamtrack(track - 1)->test(sector))
        return false;

    bamtrack(track - 1)->reset(sector); // mark track sector as ALLOCATED 
    bamtrack(track - 1)->free--;        // decrement free

    return true;
}

/// <summary>
/// Find and allocate a sector in provided track
/// </summary>
/// <param name="track">track to search for free sector</param>
/// <param name="sector">out found sector if sucessful</param>
/// <returns>true if successful</returns>
bool d64::findAndAllocateFreeOnTrack(int track, int& sector)
{
    if (track < 1 || track > TRACKS) {
        throw std::runtime_error("Invalid Tack TRACK:" + std::to_string(track));
    }

    // if there are no free sectors in the track go to next track
    if (bamtrack(track - 1)->free < 1) 
        return false;

    auto start_sector = (lastSectorUsed[track - 1] + INTERLEAVE) % SECTORS_PER_TRACK[track - 1];

    // find the free sector
    for (auto i = 0; i < SECTORS_PER_TRACK[track - 1]; ++i) {
        int search_sector = (start_sector + i) % SECTORS_PER_TRACK[track - 1]; // Wrap around

        // see if sector is free
        if (bamtrack(track - 1)->test(search_sector)) {
            allocateSector(track, search_sector);
            sector = search_sector;
            // update the last sector used for the track
            lastSectorUsed[track - 1] = sector;
            return true;
        }
    }
    return false;
}

/// <summary>
/// Find and allocate a sector
/// </summary>
/// <param name="track">out track number</param>
/// <param name="sector">out sector number</param>
/// <param name="directory">true if allocating s directory emtry/param>
/// <returns>true if successful</returns>
bool d64::findAndAllocateFreeSector(int& track, int& sector, bool directory)
{
    static const std::array<int, 40> TRACK_40_SEARCH_ORDER = {
        18, 17, 19, 16, 20, 15, 21, 14, 22, 13, 23, 12, 24, 11, 25, 10, 26, 9,
        27, 8, 28, 7, 29, 6, 30, 5, 31, 4, 32, 3, 33, 2, 34, 1, 35, 36, 37, 38, 39, 40
    };

    for (auto& t : TRACK_40_SEARCH_ORDER) {
        if (disktype == diskType::thirty_five_track && t > TRACKS_35)
            continue;

        if (t == DIRECTORY_TRACK) {
            if (!directory) {
                continue;
            }
		}
        else if (directory) {
            continue;
        }
        if (findAndAllocateFreeOnTrack(t, sector)) {
            track = t;
            return true;
        }
    }

    return false;
}

/// <summary>
/// Get the number of free sectors
/// </summary>
/// <returns>number of free sectors</returns>
uint16_t d64::getFreeSectorCount()
{
    // init free to 0
    uint16_t free = 0;

    // loop through tracks
    for (auto t = 1; t <= TRACKS; ++t) {

        // skip directory track
        if (t == DIRECTORY_TRACK)
            continue;

        // add the free bytes of each track
        free += static_cast<uint16_t>(bamtrack(t - 1)->free);
    }
    return free;
}

/// <summary>
/// Initialize the fields of BAM to default values
/// set the name of the disk
/// </summary>
/// <param name="bamPtr"></param>
/// <param name="name"></param>
void d64::initializeBAMFields(std::string_view name)
{
    if (!diskBamPtr) {
        throw std::runtime_error("Invalid BAM pointer");
    }

    diskBamPtr->dirStart.track = DIRECTORY_TRACK;
    diskBamPtr->dirStart.sector = DIRECTORY_SECTOR;

    diskBamPtr->dosVersion = DOS_VERSION;
    diskBamPtr->unused = 0;

    auto len = std::min(name.size(), static_cast<size_t>(DISK_NAME_SZ));
    std::fill_n(diskBamPtr->diskName, DISK_NAME_SZ, static_cast<char>(A0_VALUE));
    if (!name.empty() && len > 0) {
        for (size_t i = 0; i < len; ++i) {
            diskBamPtr->diskName[i] = name[i];
        }
    }

    diskBamPtr->a0[0] = A0_VALUE;
    diskBamPtr->a0[1] = A0_VALUE;

    diskBamPtr->diskId[0] = A0_VALUE;
    diskBamPtr->diskId[1] = A0_VALUE;
    diskBamPtr->unused2 = A0_VALUE;

    diskBamPtr->dos_type[0] = DOS_TYPE;
    diskBamPtr->dos_type[1] = DOS_VERSION;

    std::fill_n(diskBamPtr->unused3, UNUSED3_SZ, 0x00);
    std::fill_n(diskBamPtr->unused4, UNUSED4_SZ, 0x00);
}

/// <summary>
/// Trim a file name
/// stops at A0 padding
/// </summary>
/// <param name="file_name">name to trim</param>
/// <returns>trimmed name</returns>
std::string d64::Trim(const char filename[FILE_NAME_SZ])
{
    std::string name(filename, FILE_NAME_SZ);
    name.erase(std::find_if(name.rbegin(), name.rend(), [](char c) { return c != static_cast<char>(A0_VALUE); }).base(), name.end());
    return name;
}

/// <summary>
/// Move a file to the top of the directory list
/// </summary>
/// <param name="file">File to move</param>
/// <returns>true on success</returns>
bool d64::movefileFirst(std::string file)
{
    std::vector<directoryEntry> files = directory();
    auto it = std::find_if(files.begin(), files.end(), [&](const directoryEntry& entry)
        {
            return Trim(entry.fileName) == file;
        });

    if (it == files.end() || it == files.begin())
        return false;  // File not found or already at the top

    std::iter_swap(files.begin(), it);
    return reorderDirectory(files);
}

/// <summary>
/// Lock/unlock a file
/// </summary>
/// <param name="filename">file to lock</param>
/// <returns>true on success</returns>
bool d64::lockfile(std::string filename, bool lock)
{
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value()) {
        throw std::runtime_error("File not found " + filename);
    }
    fileEntry.value()->file_type.locked = lock ? 1 : 0;
    return true;
}

/// <summary>
/// Reorder the directory by the vector for directory entrys
/// </summary>
/// <param name="files">vector of directory entries</param>
/// <returns>true on success</returns>
bool d64::reorderDirectory(std::vector<directoryEntry>& files)
{
    std::vector<directoryEntry> currentFiles = directory();

    if (currentFiles == files)
        return false;  // No need to rewrite if already in the correct order

    int dir_track = DIRECTORY_TRACK;
    int dir_sector = DIRECTORY_SECTOR;
    auto dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
    size_t index = 0;

    while (dir_track != 0 && index < files.size()) {
        std::fill_n(reinterpret_cast<uint8_t*>(dirSectorPtr), SECTOR_SIZE, 0); // Clear sector

        auto len = std::min(FILES_PER_SECTOR, static_cast<int>(files.size() - index));
        std::copy_n(files.begin() + index, len, dirSectorPtr->fileEntry);
        index += len;

        dir_track = dirSectorPtr->next.track;
        dir_sector = dirSectorPtr->next.sector;
        if (dir_track != 0) {
            dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
        }
    }
    return true;
}

/// <summary>
/// reorder a directory based on a compare function
/// </summary>
/// <param name="compare">function comparer for directory entries</param>
/// <returns>true on success</returns>
bool d64::reorderDirectory(std::function<bool(const directoryEntry&, const directoryEntry&)> compare)
{
    // get the current directory entries
    std::vector<directoryEntry> files = directory();

    if (files.empty())
        return false; // No files to reorder

    // Sort files based on user-defined comparison function**
    std::sort(files.begin(), files.end(), compare);

    // reorder based on sorted list
    return reorderDirectory(files);
}

/// <summary>
/// return the current directory entries
/// </summary >
/// <returns>current directory entries</returns>
std::vector<directoryEntry> d64::directory()
{
    std::vector<directoryEntry> files;

    int dir_track = DIRECTORY_TRACK;
    int dir_sector = DIRECTORY_SECTOR;

    // Read all directory entries
    while (dir_track != 0) {
        auto dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

        std::copy_if(dirSectorPtr->fileEntry,
            dirSectorPtr->fileEntry + FILES_PER_SECTOR,
            std::back_inserter(files), [&](auto& entry) { return entry.file_type.closed; });

        dir_track = dirSectorPtr->next.track;
        dir_sector = dirSectorPtr->next.sector;
    }
    return files;
}

/// <summary>
/// Validate that this is a .d64 disk
/// </summary>
/// <returns>true if valid</returns>
bool d64::validateD64()
{
    auto sz = disktype == diskType::thirty_five_track ? D64_DISK35_SZ : D64_DISK40_SZ;
    if (data.size() != sz) {
        std::cerr << "Error: Invalid .d64 size (" << data.size() << " bytes), expected " << sz << " bytes\n";
        return false;
    }

    if (diskBamPtr->dirStart.track != DIRECTORY_TRACK || diskBamPtr->dirStart.sector != DIRECTORY_SECTOR) {
        std::cerr << "Warning: BAM structure has non-standard directory track/sector ("
                  << static_cast<int>(diskBamPtr->dirStart.track) << "/"
                  << static_cast<int>(diskBamPtr->dirStart.sector) << "). "
                  << "This may be intentional for copy protection or custom formats.\n";
    }

    auto dir = getTrackSectorPtr(DIRECTORY_TRACK, DIRECTORY_SECTOR);
    auto valid = (dir->track == DIRECTORY_TRACK || dir->track == 0);
    if (!valid) {
        std::cerr << "Warning: Directory sector pointer has non-standard next track (" 
                  << static_cast<int>(dir->track) << ").\n";
    }

    return true;
}

/// <summary>
/// Read side sectors of .REL file
/// </summary>
/// <param name="sideTrack">side track</param>
/// <param name="sideSector">side sector</param>
/// <returns>true if successful</returns>
std::vector<trackSector> d64::parseSideSectors(int sideTrack, int sideSector)
{
    std::vector<trackSector> recordMap;

    while (sideTrack != 0) {
        auto sideSectorPtr = getSideSectorPtr(sideTrack, sideSector);

        uint8_t nextTrack = sideSectorPtr->next.track;
        uint8_t nextSector = sideSectorPtr->next.sector;

        for (auto i = 0; i < SIDE_SECTOR_CHAIN_SZ; ++i) {
            if (sideSectorPtr->chain[i].track == 0)
                break;

            recordMap.emplace_back(sideSectorPtr->chain[i]);
        }

        sideTrack = nextTrack;
        sideSector = nextSector;
    }
    return recordMap;
}

/// <summary>
/// Write data to disk
/// </summary>
/// <param name="track">tarck to write</param>
/// <param name="sector">sector to write</param>
/// <param name="bytes">vector of bytes to write</param>
/// <param name="byteoffset">offset of sector write at</param>
/// <returns></returns>
bool d64::writeData(int track, int sector, std::vector<uint8_t> bytes, int byteoffset = 0)
{
    if (byteoffset < 0 || byteoffset >= SECTOR_SIZE) return false;
    auto index = calcOffset(track, sector) + byteoffset;
    if (index >= 0 && index + bytes.size() <= data.size()) {
        std::copy(bytes.begin(), bytes.end(), data.begin() + index);
        return true;
    }
    return false;
}

int d64::getRecordCount(std::string_view filename) {
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value() || fileEntry.value()->file_type.type != d64FileTypes::REL) return 0;
    
    int recordLength = fileEntry.value()->recordLength;
    if (recordLength == 0) return 0;
    
    int totalPayloadBytes = 0;
    trackSector sidePosition = fileEntry.value()->side;
    
    while (sidePosition.track != 0) {
        auto side = getSideSectorPtr(sidePosition.track, sidePosition.sector);
        for (int i = 0; i < SIDE_SECTOR_CHAIN_SZ; ++i) {
            if (side->chain[i].track == 0) break;
            auto dataSectorPos = side->chain[i];
            auto dataSector = getSectorPtr(dataSectorPos.track, dataSectorPos.sector);
            if (dataSector->next.track != 0) {
                totalPayloadBytes += 254;
            } else {
                totalPayloadBytes += dataSector->next.sector - 1; 
                break;
            }
        }
        sidePosition = side->next;
    }
    
    return totalPayloadBytes / recordLength;
}

int d64::getRecordSize(std::string_view filename) {
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value() || fileEntry.value()->file_type.type != d64FileTypes::REL) return 0;
    return fileEntry.value()->recordLength;
}

std::optional<std::vector<uint8_t>> d64::readRecord(std::string_view filename, int recordNumber) {
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value() || fileEntry.value()->file_type.type != d64FileTypes::REL) return std::nullopt;
    
    int recordLength = fileEntry.value()->recordLength;
    if (recordLength == 0) return std::nullopt;
    
    int recordCount = getRecordCount(filename);
    if (recordNumber < 1 || recordNumber > recordCount) return std::nullopt;
    
    int byteOffset = (recordNumber - 1) * recordLength;
    int sectorIndex = byteOffset / 254;
    int byteOffsetInSector = (byteOffset % 254) + 2;
    
    int sideSectorIndex = sectorIndex / 120;
    int pointerIndex = sectorIndex % 120;
    
    trackSector sidePosition = fileEntry.value()->side;
    sideSectorPtr side = nullptr;
    
    for (int i = 0; i <= sideSectorIndex; ++i) {
        if (sidePosition.track == 0) return std::nullopt;
        side = getSideSectorPtr(sidePosition.track, sidePosition.sector);
        sidePosition = side->next;
    }
    
    if (!side) return std::nullopt;
    trackSector dataSectorPos = side->chain[pointerIndex];
    if (dataSectorPos.track == 0) return std::nullopt;
    
    auto dataSector = getSectorPtr(dataSectorPos.track, dataSectorPos.sector);
    std::vector<uint8_t> recordData(recordLength);
    
    int bytesToRead = recordLength;
    int currentOffset = byteOffsetInSector;
    int recordOffset = 0;
    
    while (bytesToRead > 0) {
        int bytesAvailableInSector = 256 - currentOffset;
        
        int bytesToCopy = std::min(bytesToRead, bytesAvailableInSector);
        
        std::copy_n(dataSector->data.begin() + currentOffset - 2, bytesToCopy, recordData.begin() + recordOffset);
        
        bytesToRead -= bytesToCopy;
        recordOffset += bytesToCopy;
        
        if (bytesToRead > 0) {
            dataSectorPos = dataSector->next;
            if (dataSectorPos.track == 0) return std::nullopt;
            dataSector = getSectorPtr(dataSectorPos.track, dataSectorPos.sector);
            currentOffset = 2;
        }
    }
    
    return recordData;
}

bool d64::expandRelFile(std::string_view filename, int requiredBytes) {
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value() || fileEntry.value()->file_type.type != d64FileTypes::REL) return false;
    
    int totalPayloadBytes = 0;
    trackSector sidePosition = fileEntry.value()->side;
    sideSectorPtr side = nullptr;
    trackSector lastSidePosition = {0, 0};
    trackSector lastDataSectorPos = {0, 0};
    int lastChainIndex = -1;
    
    while (sidePosition.track != 0) {
        lastSidePosition = sidePosition;
        side = getSideSectorPtr(sidePosition.track, sidePosition.sector);
        for (int i = 0; i < SIDE_SECTOR_CHAIN_SZ; ++i) {
            if (side->chain[i].track == 0) break;
            lastDataSectorPos = side->chain[i];
            lastChainIndex = i;
            auto dataSector = getSectorPtr(lastDataSectorPos.track, lastDataSectorPos.sector);
            if (dataSector->next.track != 0) {
                totalPayloadBytes += 254;
            } else {
                totalPayloadBytes += dataSector->next.sector - 1; 
                break;
            }
        }
        sidePosition = side->next;
    }
    
    if (totalPayloadBytes >= requiredBytes) return true;
    
    int bytesToAdd = requiredBytes - totalPayloadBytes;
    
    if (lastDataSectorPos.track != 0) {
        auto lastDataSector = getSectorPtr(lastDataSectorPos.track, lastDataSectorPos.sector);
        int currentSectorSize = lastDataSector->next.sector - 1;
        if (currentSectorSize < 254) {
            int toAdd = std::min(bytesToAdd, 254 - currentSectorSize);
            std::fill_n(lastDataSector->data.begin() + currentSectorSize, toAdd, 0);
            
            bytesToAdd -= toAdd;
            currentSectorSize += toAdd;
            
            if (bytesToAdd == 0) {
                lastDataSector->next.sector = currentSectorSize + 1;
                return true;
            } else {
                lastDataSector->next.sector = 255;
            }
        }
    } else {
        return false; 
    }
    
    while (bytesToAdd > 0) {
        int nextTrack = 0, nextSector = 0;
        if (!findAndAllocateFreeSector(nextTrack, nextSector, false)) {
            return false;
        }
        
        auto lastDataSector = getSectorPtr(lastDataSectorPos.track, lastDataSectorPos.sector);
        lastDataSector->next.track = nextTrack;
        lastDataSector->next.sector = nextSector;
        
        lastDataSectorPos = {static_cast<uint8_t>(nextTrack), static_cast<uint8_t>(nextSector)};
        auto newDataSector = getSectorPtr(lastDataSectorPos.track, lastDataSectorPos.sector);
        std::fill(newDataSector->data.begin(), newDataSector->data.end(), 0); // Fill with zeros
        
        int toAdd = std::min(bytesToAdd, 254);
        bytesToAdd -= toAdd;
        
        newDataSector->next.track = 0;
        newDataSector->next.sector = toAdd + 1;
        
        lastChainIndex++;
        if (lastChainIndex >= SIDE_SECTOR_CHAIN_SZ) {
            int newSideTrack = 0, newSideSector = 0;
            if (!findAndAllocateFreeSector(newSideTrack, newSideSector, false)) return false;
            
            side->next.track = newSideTrack;
            side->next.sector = newSideSector;
            
            auto newSide = getSideSectorPtr(newSideTrack, newSideSector);
            std::fill(reinterpret_cast<uint8_t*>(newSide), reinterpret_cast<uint8_t*>(newSide) + SECTOR_SIZE, 0);
            newSide->recordsize = fileEntry.value()->recordLength;
            newSide->block = side->block + 1;
            
            int sideSectorsCount = newSide->block + 1;
            if (sideSectorsCount > SIDE_SECTOR_ENTRY_SIZE) return false;
            
            trackSector iterSidePos = fileEntry.value()->side;
            while (iterSidePos.track != 0) {
                auto iterSide = getSideSectorPtr(iterSidePos.track, iterSidePos.sector);
                iterSide->sideSectors[newSide->block] = {static_cast<uint8_t>(newSideTrack), static_cast<uint8_t>(newSideSector)};
                iterSidePos = iterSide->next;
            }
            
            side = newSide;
            lastChainIndex = 0;
            
            uint16_t currentSize = fileEntry.value()->fileSize[0] | (fileEntry.value()->fileSize[1] << 8);
            currentSize++;
            fileEntry.value()->fileSize[0] = currentSize & 0xFF;
            fileEntry.value()->fileSize[1] = currentSize >> 8;
        }
        
        side->chain[lastChainIndex] = lastDataSectorPos;
        
        uint16_t currentSize = fileEntry.value()->fileSize[0] | (fileEntry.value()->fileSize[1] << 8);
        currentSize++;
        fileEntry.value()->fileSize[0] = currentSize & 0xFF;
        fileEntry.value()->fileSize[1] = currentSize >> 8;
    }
    
    return true;
}

bool d64::writeRecord(std::string_view filename, int recordNumber, const std::vector<uint8_t>& recordData) {
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value() || fileEntry.value()->file_type.type != d64FileTypes::REL) return false;
    
    int recordLength = fileEntry.value()->recordLength;
    if (recordLength == 0) return false;
    
    if (recordData.size() != static_cast<size_t>(recordLength)) return false;
    if (recordNumber < 1) return false;
    
    int byteOffset = (recordNumber - 1) * recordLength;
    int requiredBytes = byteOffset + recordLength;
    
    if (!expandRelFile(filename, requiredBytes)) return false;
    
    int sectorIndex = byteOffset / 254;
    int byteOffsetInSector = (byteOffset % 254) + 2;
    int sideSectorIndex = sectorIndex / 120;
    int pointerIndex = sectorIndex % 120;
    
    trackSector sidePosition = fileEntry.value()->side;
    sideSectorPtr side = nullptr;
    
    for (int i = 0; i <= sideSectorIndex; ++i) {
        if (sidePosition.track == 0) return false;
        side = getSideSectorPtr(sidePosition.track, sidePosition.sector);
        sidePosition = side->next;
    }
    
    if (!side) return false;
    trackSector dataSectorPos = side->chain[pointerIndex];
    if (dataSectorPos.track == 0) return false;
    
    auto dataSector = getSectorPtr(dataSectorPos.track, dataSectorPos.sector);
    
    int bytesToWrite = recordLength;
    int currentOffset = byteOffsetInSector;
    int recordOffset = 0;
    
    while (bytesToWrite > 0) {
        int bytesAvailableInSector = 256 - currentOffset;
        int bytesToCopy = std::min(bytesToWrite, bytesAvailableInSector);
        
        std::copy_n(recordData.begin() + recordOffset, bytesToCopy, dataSector->data.begin() + currentOffset - 2);
        
        bytesToWrite -= bytesToCopy;
        recordOffset += bytesToCopy;
        
        if (bytesToWrite > 0) {
            dataSectorPos = dataSector->next;
            if (dataSectorPos.track == 0) return false; 
            dataSector = getSectorPtr(dataSectorPos.track, dataSectorPos.sector);
            currentOffset = 2;
        }
    }
    
    return true;
}

bool d64::appendRecord(std::string_view filename, const std::vector<uint8_t>& recordData) {
    int count = getRecordCount(filename);
    return writeRecord(filename, count + 1, recordData);
}

bool d64::deleteRecord(std::string_view filename, int recordNumber) {
    int recordLength = getRecordSize(filename);
    if (recordLength == 0) return false;
    std::vector<uint8_t> blankRecord(recordLength, 0x00);
    blankRecord[0] = 0xFF;
    return writeRecord(filename, recordNumber, blankRecord);
}

