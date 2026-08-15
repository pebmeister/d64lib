#include <gtest/gtest.h>
#include "../d64.h"
#include "../geos.h"
#include <vector>
#include <fstream>
#include <cstdio>
#include <iostream>

using namespace d64lib;
using namespace d64lib::geos;

namespace {

    // Helper to create an empty test d64 file
    void create_test_d64(const char* name) {
        std::ofstream file(name, std::ios::binary);
        for (int i = 0; i < 174848; ++i) {
            file.put('\0');
        }
    }

    void d64lib_unit_test_method_initialize() {
        create_test_d64("FILENAME");
        create_test_d64("DUMMYFILE");
        d64 d;
        d.formatDisk("FILENAME");
    }

    void d64lib_unit_test_method_cleanup(d64& d) {
        std::remove("FILENAME");
        std::remove("DUMMYFILE");
    }

    TEST(geos_unit_test, format_geos_disk_test) {
        d64lib_unit_test_method_initialize();
        
        d64 disk;
        bool res = formatGeosDisk(disk, "GEOSDISK");
        EXPECT_TRUE(res);
        
        EXPECT_TRUE(isGeosDisk(disk));
        
        d64 disk2;
        disk2.formatDisk("NORMALDISK");
        EXPECT_FALSE(isGeosDisk(disk2));
        
        d64lib_unit_test_method_cleanup(disk);
    }
    
    TEST(geos_unit_test, read_info_block_test) {
        d64lib_unit_test_method_initialize();
        d64 disk;
        
        // We will mock an Info Block and insert it artificially via low-level primitives
        // to test the parsing logic.
        std::vector<uint8_t> dummyData(10, 0);
        disk.addFile("TESTGEOS", c64FileType(d64FileTypes::PRG), dummyData); // Base file
        
        auto entry = disk.findFile("TESTGEOS");
        ASSERT_TRUE(entry.has_value());
        
        // Allocate a sector for the info block manually
        int infoTrack, infoSector;
        ASSERT_TRUE(disk.findAndAllocateFreeSector(infoTrack, infoSector));
        
        // Set info pointer directly via the directoryEntry pointer since it maps to disk memory
        entry.value()->side.track = infoTrack; // GEOS side.track overrides standard
        entry.value()->side.sector = infoSector; // GEOS side.sector
        // 0x16 is unused[0], 0x17 is unused[1], 0x18 is unused[2].
        // GEOS type at 0x18
        entry.value()->unused[2] = 0x06; // Application type = 6 (GeosFileType)
        entry.value()->unused[1] = 0x01; // VLIR
        
        // Now craft the info block
        std::vector<uint8_t> infoBlock(256, 0);
        infoBlock[0x02] = 3;
        for (int i = 0; i < 63; i++) infoBlock[0x03 + i] = 0xAA;
        infoBlock[0x44] = 0x82; // PRG DOS Type
        infoBlock[0x45] = 0x06; // Application
        infoBlock[0x46] = 0x01; // VLIR
        infoBlock[0x47] = 0x00; infoBlock[0x48] = 0x04; // $0400 load
        
        std::string author = "GEOS DEV";
        for (size_t i = 0; i < author.length(); i++) infoBlock[0x61 + i] = author[i];

        std::string classname = "TEST CLASS";
        for (size_t i = 0; i < classname.length(); i++) infoBlock[0x4D + i] = classname[i];

        disk.writeSector(infoTrack, infoSector, infoBlock);
        
        auto info = readInfoBlock(disk, "TESTGEOS");
        ASSERT_TRUE(info.has_value());
        
        // Read via cache to avoid filesystem delay
        EXPECT_EQ(info.value().dosType, 0x82);
        EXPECT_EQ(info.value().geosType, FileType::Application);
        EXPECT_EQ(info.value().structure, FileStructure::Vlir);
        EXPECT_EQ(info.value().loadAddress, 0x0400);
        EXPECT_EQ(info.value().author, "GEOS DEV");
        EXPECT_EQ(info.value().className, "TEST CLASS");
        EXPECT_EQ(info.value().iconWidth, 3);
        EXPECT_EQ(info.value().iconHeight, 21);
        EXPECT_EQ(info.value().iconData.size(), 63u);
        EXPECT_EQ(info.value().iconData[0], 0xAA);
        
        d64lib_unit_test_method_cleanup(disk);
    }
    
    TEST(geos_unit_test, vlir_record_test) {
        d64lib_unit_test_method_initialize();
        d64 disk;
        
        // Base setup:
        std::vector<uint8_t> dummyData(1, 0);
        disk.addFile("VLIRTEST", c64FileType(d64FileTypes::PRG), dummyData);
        auto entry = disk.findFile("VLIRTEST");
        ASSERT_TRUE(entry.has_value());
        
        // Let's create an index sector
        int idxTrack, idxSector;
        disk.findAndAllocateFreeSector(idxTrack, idxSector);
        
        // Create 2 records
        int r1T, r1S, r2T, r2S;
        disk.findAndAllocateFreeSector(r1T, r1S);
        disk.findAndAllocateFreeSector(r2T, r2S);
        
        std::vector<uint8_t> r1Data(256, 0);
        r1Data[0] = 0x00; r1Data[1] = 0x05; // 3 payload bytes
        r1Data[2] = 0x11; r1Data[3] = 0x22; r1Data[4] = 0x33;
        disk.writeSector(r1T, r1S, r1Data);
        
        std::vector<uint8_t> r2Data(256, 0);
        r2Data[0] = 0x00; r2Data[1] = 0x04; // 2 payload bytes
        r2Data[2] = 0xAA; r2Data[3] = 0xBB;
        disk.writeSector(r2T, r2S, r2Data);
        
        std::vector<uint8_t> indexData(256, 0);
        indexData[0] = 0x00; indexData[1] = 0xFF; // GEOS index block ends immediately
        indexData[2] = r1T; indexData[3] = r1S; // Record 0
        indexData[4] = 0x00; indexData[5] = 0x00; // Record 1 empty
        indexData[6] = r2T; indexData[7] = r2S; // Record 2
        disk.writeSector(idxTrack, idxSector, indexData);
        
        // Point directory to index sector directly in memory
        entry.value()->start.track = idxTrack;
        entry.value()->start.sector = idxSector;
        
        // Test parsing
        int count = getVlirRecordCount(disk, "VLIRTEST");
        EXPECT_EQ(count, 3); // Max index is 2, so count is 3.

        auto rec0 = readVlirRecord(disk, "VLIRTEST", 0);
        ASSERT_TRUE(rec0.has_value());
        EXPECT_EQ(rec0.value().size(), 3);
        EXPECT_EQ(rec0.value()[0], 0x11);
        
        auto rec1 = readVlirRecord(disk, "VLIRTEST", 1);
        ASSERT_FALSE(rec1.has_value()); // empty
        
        auto rec2 = readVlirRecord(disk, "VLIRTEST", 2);
        ASSERT_TRUE(rec2.has_value());
        EXPECT_EQ(rec2.value().size(), 2);
        EXPECT_EQ(rec2.value()[0], 0xAA);
        EXPECT_EQ(rec2.value()[1], 0xBB);
        
        d64lib_unit_test_method_cleanup(disk);
    }
}
