// Written by Paul Baxter
#include <gtest/gtest.h>
#include <string>

#include "d64.h"

#pragma warning(disable:4996)

namespace d64lib_unit_test
{
    static void d64lib_unit_test_method_initialize();
    static void d64lib_unit_test_method_cleanup();

    static void d64lib_unit_test_method_initialize()
    {
    }

    static void d64lib_unit_test_method_cleanup()
    {
    }

    std::string getPaddingString(std::string const& str, int n)
    {
        std::ostringstream oss;
        oss << std::left << std::setw(n) << str;
        return oss.str();
    }

    void allocation_helper(d64* disk)
    {
        std::array<std::array<bool, 21>, TRACKS_40> sectorUsage = {}; // Max sectors per track
        std::array<uint8_t, TRACKS_40> trackFreeUsage = {};

        std::copy_n(disk->SECTORS_PER_TRACK.begin(), TRACKS_40, trackFreeUsage.begin());

        sectorUsage[DIRECTORY_TRACK - 1][DIRECTORY_SECTOR] = true;
        sectorUsage[DIRECTORY_TRACK - 1][BAM_SECTOR] = true;
        trackFreeUsage[DIRECTORY_TRACK - 1] -= 2;
        auto expected_free_sectors = disk->TRACKS == TRACKS_35 ?
            (D64_DISK35_SZ / SECTOR_SIZE) - disk->SECTORS_PER_TRACK[DIRECTORY_TRACK - 1] :
            (D64_DISK40_SZ / SECTOR_SIZE) - disk->SECTORS_PER_TRACK[DIRECTORY_TRACK - 1];

        auto count = disk->getFreeSectorCount();
        std::vector<TrackSector> allocations;
        for (auto allocation = 0; allocation < count; ++allocation) {
            int track, sector;

            if (disk->findAndAllocateFreeSector(track, sector)) {
                if (track != DIRECTORY_TRACK) {
                    --expected_free_sectors;
                }
                trackFreeUsage[track - 1] -= 1;
                sectorUsage[track - 1][sector] = true;

                count = disk->getFreeSectorCount();
                EXPECT_EQ(count, expected_free_sectors);
                for (auto track = 1; track <= disk->TRACKS; ++track) {
                    EXPECT_EQ(trackFreeUsage[track - 1], disk->bamtrack(track - 1)->free);
                    for (auto s = 0; s < disk->SECTORS_PER_TRACK[track - 1]; ++s) {
                        EXPECT_NE(disk->bamtrack(track - 1)->test(s), sectorUsage[track - 1][s]);
                    }
                }

                TrackSector ts(track, sector);
                auto it = std::find(allocations.begin(), allocations.end(), ts);
                EXPECT_EQ(it, allocations.end());
                allocations.push_back(ts);
            }
        }
    }


    TEST(d64lib_unit_test, sector_allocation_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        allocation_helper(&disk);

        disk.save("sector_allocation_test.d64");
        d64lib_unit_test_method_cleanup();
    }


    TEST(d64lib_unit_test, sector_allocation_40_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk(diskType::forty_track);
        allocation_helper(&disk);

        disk.save("sector_allocation_40_test.d64");
        d64lib_unit_test_method_cleanup();
    }


    TEST(d64lib_unit_test, create_unit_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        EXPECT_STREQ(disk.diskname().c_str(), "NEW DISK");
        EXPECT_EQ(disk.TRACKS, TRACKS_35);
        auto dir = disk.directory();
        EXPECT_TRUE(dir.size() == 0);
        EXPECT_TRUE(disk.verifyBAMIntegrity(false, ""));
        EXPECT_EQ(disk.getFreeSectorCount(), (D64_DISK35_SZ / SECTOR_SIZE) - 
            disk.SECTORS_PER_TRACK[DIRECTORY_TRACK -1]);

        disk.save("create_unit_test.d64");
        d64lib_unit_test_method_cleanup();
    }


    TEST(d64lib_unit_test, create_40_unit_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk(diskType::forty_track);
        EXPECT_STREQ(disk.diskname().c_str(), "NEW DISK");
        EXPECT_EQ(disk.TRACKS, TRACKS_40);
        auto dir = disk.directory();
        EXPECT_TRUE(dir.size() == 0);
        EXPECT_TRUE(disk.verifyBAMIntegrity(false, ""));
        EXPECT_EQ(disk.getFreeSectorCount(), (D64_DISK40_SZ / SECTOR_SIZE) - 
            disk.SECTORS_PER_TRACK[DIRECTORY_TRACK - 1]);

        disk.save("create_40_unit_test.d64");
        d64lib_unit_test_method_cleanup();
    }


    TEST(d64lib_unit_test, addrelfile_test)
    {
        d64lib_unit_test_method_initialize();
        constexpr int RECORD_SIZE = 64;
        constexpr int NUM_RECORDS = 200;

        std::vector<uint8_t> rel_file;
        for (auto record = 0; record < NUM_RECORDS; ++record) {
            std::string rec =  getPaddingString(std::string("RECORD ") + std::to_string(record + 1), RECORD_SIZE);
            rel_file.insert(rel_file.end(), rec.begin(), rec.end());
        }

        d64 disk;
        auto added = disk.addRelFile("RELFILE", FileTypes::REL, RECORD_SIZE,  rel_file);
        EXPECT_TRUE(added);

        disk.save("addrelfile_test.d64");
        d64lib_unit_test_method_cleanup();
    }

    TEST(d64lib_unit_test, readrelfile_test)
    {
        d64lib_unit_test_method_initialize();
        constexpr int RECORD_SIZE = 64;
        constexpr int NUM_RECORDS = 200;

        std::vector<uint8_t> rel_file;
        for (auto record = 0; record < NUM_RECORDS; ++record) {
            std::string rec = getPaddingString(std::string("RECORD ") + std::to_string(record + 1), RECORD_SIZE);
            rel_file.insert(rel_file.end(), rec.begin(), rec.end());
        }

        d64 disk;
        auto added = disk.addRelFile("RELFILE", FileTypes::REL, 64, rel_file);
        EXPECT_TRUE(added);

        auto readrelfile = disk.readFile("RELFILE");

        EXPECT_TRUE(readrelfile.has_value());
        if (readrelfile.has_value()) {
            EXPECT_EQ(rel_file, readrelfile.value());
        }

        disk.save("readrelfile_test.d64");
        d64lib_unit_test_method_cleanup();
    }

    TEST(d64lib_unit_test, large_file_unit_test)
    {
        d64lib_unit_test_method_initialize();

        const auto bigSize = 90000;

        std::vector<uint8_t> big_file(bigSize);
        for (auto i = 0; i < bigSize; ++i) {
            big_file[i] = i % 256;
        }

        d64 disk;
        auto added = disk.addFile("BIG", FileTypes::SEQ, big_file);
        EXPECT_TRUE(added);

        auto readfile = disk.readFile("BIG");
        EXPECT_TRUE(readfile.has_value());
        if (readfile.has_value()) {
            for (auto i = 0; i < bigSize; ++i) {
                EXPECT_TRUE(readfile.value()[i] == big_file[i]);
            }
        }
        disk.save("large_file_unit_test.d64");
        d64lib_unit_test_method_cleanup();
    }


    TEST(d64lib_unit_test, add_file_unit_test)
    {
        d64lib_unit_test_method_initialize();

        std::vector<uint8_t> prog = {
0x01, 0x08, 0x0f, 0x08, 0x0a, 0x00, 0x99, 0x20, 0x22, 0x48, 0x45, 0x4c, 0x4c, 0x4f, 0x22, 0x00,
0x1b, 0x08, 0x14, 0x00, 0x81, 0x4b, 0xb2, 0x31, 0xa4, 0x31, 0x30, 0x00, 0x27, 0x08, 0x1e, 0x00,
0x81, 0x4c, 0xb2, 0x4b, 0xa4, 0x31, 0x31, 0x00, 0x31, 0x08, 0x28, 0x00, 0x99, 0x20, 0x4b, 0x2c,
0x4c, 0x00, 0x39, 0x08, 0x32, 0x00, 0x82, 0x3a, 0x82, 0x00, 0x3f, 0x08, 0x3c, 0x00, 0x80, 0x00,
0x00, 0x00
        };

        d64 disk;
        for (auto file = 1; disk.getFreeSectorCount() > 5; ++file) {
            auto numpart = std::to_string(file);

            std::string filename = "FILE";
            filename += numpart;
            auto added = disk.addFile(filename, FileTypes::PRG, prog);
            EXPECT_TRUE(added);
            auto dir = disk.directory();
            EXPECT_TRUE(dir.size() == file);

            auto readfile = disk.readFile(filename);
            EXPECT_TRUE(readfile.has_value());
            if (readfile.has_value()) {
                EXPECT_TRUE(readfile.value() == prog);
            }
        }
        disk.save("add_file_unit_test.d64");
        d64lib_unit_test_method_cleanup();
    }


    TEST(d64lib_unit_test, extract_file_unit_test)
    {
        d64lib_unit_test_method_initialize();

        std::vector<uint8_t> prog = {
0x01, 0x08, 0x15, 0x08, 0x0a, 0x00, 0x99, 0x20, 0x22, 0x48, 0x45, 0x4c, 0x4c, 0x4f, 0x20,
0x57, 0x4f, 0x52, 0x4c, 0x44, 0x22, 0x00, 0x1b, 0x08, 0x14, 0x00, 0x80, 0x00, 0x00, 0x00
        };

        d64 disk;
        std::vector<std::string> files;

        for (auto file = 1; disk.getFreeSectorCount() > 5; ++file) {
            auto numpart = std::to_string(file);

            std::string filename = "FILE";
            filename += numpart;
            auto added = disk.addFile(filename, FileTypes::PRG, prog);
            EXPECT_TRUE(added);
            auto dir = disk.directory();
            EXPECT_TRUE(dir.size() == file);
            files.push_back(filename);
        }

        for (auto& filename : files) {
            std::cout << "File " << filename << "\n";
            auto extracted = disk.extractFile(filename);
            EXPECT_TRUE(extracted);
            if (extracted) {
                std::remove((filename + ".prg").c_str());
            }
        }        
        disk.save("extract_file_unit_test.d64");
        d64lib_unit_test_method_cleanup();
    }
}