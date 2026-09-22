/**
 * @file geos.h
 * @brief GEOS (Graphic Environment Operating System) file and disk support for d64lib.
 *
 * Provides structures and utilities for parsing GEOS disk formats, application Info Blocks,
 * and VLIR (Variable Length Index Record) file structures on Commodore 64 disk images.
 */

#pragma once

#include "d64.h"
#include <string>
#include <vector>
#include <string_view>
#include <optional>
#include <cstdint>
#include <array>

/**
 * @namespace d64lib::geos
 * @brief Contains classes, structures, and routines for handling GEOS-formatted C64 disks and files.
 */
namespace d64lib::geos {

/**
 * @enum FileType
 * @brief GEOS file type identifiers stored in the application Info Block.
 */
enum class FileType : uint8_t {
    NonGeos = 0x00,           /**< Standard non-GEOS file */
    Basic = 0x01,             /**< GEOS BASIC program */
    Assembly = 0x02,          /**< Assembly language source/object */
    Data = 0x03,              /**< General data file */
    System = 0x04,            /**< GEOS system file */
    DeskAccessory = 0x05,     /**< Desk accessory application */
    Application = 0x06,       /**< GEOS application program */
    ApplicationData = 0x07,   /**< Application-specific data file */
    Font = 0x08,              /**< GEOS system font file */
    PrinterDriver = 0x09,     /**< Printer driver */
    InputDriver = 0x0A,       /**< Input device driver (e.g., mouse/joystick) */
    DiskDriver = 0x0B,        /**< Disk drive driver */
    BootSector = 0x0C,        /**< GEOS boot sector file */
    Temporary = 0x0D,         /**< Temporary scratch file */
    AutoExecute = 0x0E        /**< Auto-execute application */
};

/**
 * @enum FileStructure
 * @brief Underlying file record architecture for GEOS files.
 */
enum class FileStructure : uint8_t {
    Sequential = 0x00, /**< Standard sequential byte stream */
    Vlir = 0x01        /**< Variable Length Index Record (VLIR) structure */
};

/**
 * @struct InfoBlock
 * @brief Represents the GEOS Info Block metadata associated with a desktop application or file.
 */
struct InfoBlock {
    uint8_t iconWidth;              /**< Width of the desktop icon in pixels */
    uint8_t iconHeight;             /**< Height of the desktop icon in pixels */
    std::vector<uint8_t> iconData;  /**< Raw pixel bitmap data for the desktop icon */
    
    uint8_t dosType;                /**< Standard Commodore DOS file type */
    FileType geosType;              /**< Specific GEOS file classification */
    FileStructure structure;        /**< File organization (Sequential or VLIR) */
    
    uint16_t loadAddress;           /**< Program starting load memory address */
    uint16_t endLoadAddress;        /**< Program ending load memory address */
    uint16_t execAddress;           /**< Program execution entry point address */
    
    std::string className;          /**< GEOS class name string */
    std::string version;            /**< Application version string */
    
    std::string author;             /**< Author or developer attribution */
    std::string description;        /**< Brief description or subtitle */
};

/**
 * @brief Check if a disk is formatted for GEOS.
 * @param disk Reference to the d64 disk instance.
 * @return true if the GEOS format string is found, false otherwise.
 */
bool isGeosDisk(d64& disk);

/**
 * @brief Format a disk and initialize the GEOS format string.
 * @param disk Reference to the d64 disk instance.
 * @param name Name of the disk.
 * @return true on success, false otherwise.
 */
bool formatGeosDisk(d64& disk, std::string_view name);

/**
 * @brief Read the Info Block for a specific GEOS file.
 * @param disk Reference to the d64 disk instance.
 * @param filename Name of the file.
 * @return An optional InfoBlock containing GEOS metadata if found.
 */
std::optional<InfoBlock> readInfoBlock(d64& disk, std::string_view filename);

/**
 * @brief Read a specific Record from a VLIR file chain.
 * @param disk Reference to the d64 disk instance.
 * @param filename Name of the file.
 * @param recordId 0-based record index (typically 0-126).
 * @return An optional byte vector containing the record payload if successful.
 */
std::optional<std::vector<uint8_t>> readVlirRecord(d64& disk, std::string_view filename, int recordId);

/**
 * @brief Read a Sequential GEOS file.
 * @param disk Reference to the d64 disk instance.
 * @param filename Name of the file.
 * @return An optional byte vector containing the file contents.
 */
std::optional<std::vector<uint8_t>> readSequentialFile(d64& disk, std::string_view filename);

/**
 * @brief Count the number of active records in a VLIR file.
 * @param disk Reference to the d64 disk instance.
 * @param filename Name of the file.
 * @return The total count of registered records.
 */
int getVlirRecordCount(d64& disk, std::string_view filename);

} // namespace d64lib::geos
