#include "pch.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <variant>
#include <typeinfo>
#include <type_traits>
#include <fstream>
#include <filesystem>

#include "glog/logging.h"

#include "LRUCache.hpp"
#include "IndexNode.hpp"
#include "DataNode.hpp"
#include "BPlusStore.hpp"
#include "LRUCacheObject.hpp"
#include "PMemStorage.hpp"
#include "TypeMarshaller.hpp"

#include "TypeUID.h"
#include "ObjectFatUID.h"
#include <set>
#include <random>
#include <numeric>

#ifdef __TREE_WITH_CACHE__
namespace BPlusStore_LRUCache_PMemStorage_Suite
{
    typedef int KeyType;
    typedef int ValueType;
    typedef ObjectFatUID ObjectUIDType;

    typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT > DataNodeType;
    typedef IndexNode<KeyType, ValueType, ObjectUIDType, DataNodeType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;

    typedef LRUCacheObject<TypeMarshaller, DataNodeType, IndexNodeType> ObjectType;
    typedef IFlushCallback<ObjectUIDType, ObjectType> ICallback;

    typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, PMemStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;

    class BPlusStore_LRUCache_PMemStorage_Suite_3 : public ::testing::TestWithParam<std::tuple<size_t, size_t, size_t, size_t, size_t, size_t>>
    {
    protected:
        void SetUp() override
        {
            std::tie(nDegree, nTotalRecords, nCacheSize, nBlockSize, nStorageSize, nThreadCount) = GetParam();

            m_ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nStorageSize, "/mnt/tmpfs/datafile1");
            m_ptrTree->init<DataNodeType>();
        }

        void TearDown() override
        {
            delete m_ptrTree;
            //std::filesystem::remove(fsTempFileStore);
        }

        BPlusStoreType* m_ptrTree;

        size_t nDegree;
        size_t nTotalRecords;
        size_t nCacheSize;
        size_t nBlockSize;
        size_t nStorageSize;
        size_t nThreadCount;

#ifdef _MSC_VER
        //std::filesystem::path fsTempFileStore = std::filesystem::temp_directory_path() / "tempfilestore.hdb";
#else //_MSC_VER
        //std::filesystem::path fsTempFileStore = "/mnt/tmpfs/filestore.hdb";
#endif //_MSC_VER

        //        std::filesystem::path fsTempFileStore = std::filesystem::temp_directory_path() / "tempfilestore.hdb";
    };

    void insert_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            ErrorCode ec = ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    void insert_concurent_reverse(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            ErrorCode ec = ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    void insert_concurent_random(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        std::vector<int> vtRandom(nRangeEnd - nRangeStart);
        std::iota(vtRandom.begin(), vtRandom.end(), nRangeStart);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (size_t nCntr = 0; nCntr < nRangeEnd - nRangeStart; nCntr++)
        {
            ErrorCode ec = ptrTree->insert(vtRandom[nCntr], vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }
    }

    void search_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }

    void search_concurent_reverese(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }

    void search_concurent_random(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        std::vector<int> vtRandom(nRangeEnd - nRangeStart);
        std::iota(vtRandom.begin(), vtRandom.end(), nRangeStart);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (size_t nCntr = 0; nCntr < nRangeEnd - nRangeStart; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(vtRandom[nCntr], nValue);

            assert(vtRandom[nCntr] == nValue && ec == ErrorCode::Success);
        }
    }

    void search_not_found_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(nCntr, nValue);

            assert(ec == ErrorCode::KeyDoesNotExist);
        }
    }

    void search_not_found_concurent_reverse(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(nCntr, nValue);

            assert(ec == ErrorCode::KeyDoesNotExist);
        }
    }

    void search_not_found_concurent_random(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        std::vector<int> vtRandom(nRangeEnd - nRangeStart);
        std::iota(vtRandom.begin(), vtRandom.end(), nRangeStart);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (size_t nCntr = 0; nCntr < nRangeEnd - nRangeStart; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = ptrTree->search(vtRandom[nCntr], nValue);

            assert(ec == ErrorCode::KeyDoesNotExist);
        }
    }

    void delete_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
        {
            ErrorCode ec = ptrTree->remove(nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    void delete_concurent_reverse(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
        {
            ErrorCode ec = ptrTree->remove(nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    void delete_concurent_random(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd)
    {
        std::vector<int> vtRandom(nRangeEnd - nRangeStart);
        std::iota(vtRandom.begin(), vtRandom.end(), nRangeStart);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (size_t nCntr = 0; nCntr < nRangeEnd - nRangeStart; nCntr++)
        {
            ErrorCode ec = ptrTree->remove(vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_LRUCache_PMemStorage_Suite_3, Bulk_Insert_v1)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_PMemStorage_Suite_3, Bulk_Insert_v2)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_reverse, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_PMemStorage_Suite_3, Bulk_Insert_v3)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_PMemStorage_Suite_3, Bulk_Search_v1)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_PMemStorage_Suite_3, Bulk_Search_v2)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_reverse, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_concurent_reverese, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_PMemStorage_Suite_3, Bulk_Search_v3)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_PMemStorage_Suite_3, Bulk_Delete_v1)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(delete_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_not_found_concurent, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_PMemStorage_Suite_3, Bulk_Delete_v2)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_reverse, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(delete_concurent_reverse, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_not_found_concurent_reverse, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

    TEST_P(BPlusStore_LRUCache_PMemStorage_Suite_3, Bulk_Delete_v3)
    {
        std::vector<std::thread> vtThreads;

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(insert_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        auto it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(delete_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }

        vtThreads.clear();

        for (int nIdx = 0; nIdx < nThreadCount; nIdx++)
        {
            int nTotal = nTotalRecords / nThreadCount;
            vtThreads.push_back(std::thread(search_not_found_concurent_random, m_ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
        }

        it = vtThreads.begin();
        while (it != vtThreads.end())
        {
            (*it).join();
            it++;
        }
    }

#ifndef _MSC_VER
#ifdef __CONCURRENT__
    INSTANTIATE_TEST_CASE_P(
        THREADED_TREE_WITH_KEY_AND_VAL_AS_INT32_AND_WITH_PMEM_STORAGE,
        BPlusStore_LRUCache_PMemStorage_Suite_3,
        ::testing::Values(
            std::make_tuple(3, 10000, 1024, 64, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(4, 10000, 1024, 64, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(5, 10000, 1024, 64, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(6, 10000, 1024, 64, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(7, 10000, 1024, 128, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(8, 10000, 1024, 128, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(15, 10000, 1024, 128, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(16, 100000, 1024, 128, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(32, 100000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(64, 100000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(128, 100000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(256, 100000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(512, 100000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(1024, 100000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8),
            std::make_tuple(2048, 100000, 1024, 256, 10ULL * 1024 * 1024 * 1024, 8)
        ));
#endif //__CONCURRENT__
#endif
}
#endif //__TREE_WITH_CACHE__
