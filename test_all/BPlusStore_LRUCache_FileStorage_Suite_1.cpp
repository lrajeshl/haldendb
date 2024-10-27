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
#include "FileStorage.hpp"
#include "TypeMarshaller.hpp"
#include "TypeUID.h"
#include "ObjectFatUID.h"
#include "IFlushCallback.h"
#include <set>
#include <random>
#include <numeric>

#ifdef __TREE_WITH_CACHE__
namespace BPlusStore_LRUCache_FileStorage_Suite
{
    typedef int KeyType;
    typedef int ValueType;

    typedef ObjectFatUID ObjectUIDType;

    typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT > DataNodeType;
    typedef IndexNode<KeyType, ValueType, ObjectUIDType, DataNodeType, TYPE_UID::INDEX_NODE_INT_INT > IndexNodeType;

    typedef LRUCacheObject<TypeMarshaller, DataNodeType, IndexNodeType> ObjectType;
    typedef IFlushCallback<ObjectUIDType, ObjectType> ICallback;

    typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, FileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;

    class BPlusStore_LRUCache_FileStorage_Suite_1 : public ::testing::TestWithParam<std::tuple<size_t, size_t, size_t, size_t, size_t>>
    {
    protected:
        void SetUp() override
        {
            std::tie(nDegree, nTotalRecords, nCacheSize, nBlockSize, nStorageSize) = GetParam();

            m_ptrTree = new BPlusStoreType(nDegree, nCacheSize, nBlockSize, nStorageSize, fsTempFileStore.string());
            m_ptrTree->init<DataNodeType>();
        }

        void TearDown() override
        {
            delete m_ptrTree;
            std::filesystem::remove(fsTempFileStore);
        }

        BPlusStoreType* m_ptrTree = nullptr;

        size_t nDegree;
        size_t nTotalRecords;
        size_t nCacheSize;
        size_t nBlockSize;
        size_t nStorageSize;

        std::filesystem::path fsTempFileStore = std::filesystem::temp_directory_path() / "tempfilestore.hdb";
    };

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Bulk_Insert_v1)
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(vtRandom[nCntr], vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Bulk_Insert_v2)
    {
        for (int nCntr = 0; nCntr < nTotalRecords; nCntr = nCntr + 2)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 1; nCntr < nTotalRecords; nCntr = nCntr + 2)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Bulk_Insert_v3)
    {
        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Bulk_Search_v1)
    {
        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Bulk_Search_v2)
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(vtRandom[nCntr], vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(vtRandom[nCntr], nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Bulk_Search_v3)
    {
        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::Success);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Bulk_Delete_v1)
    {
        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->remove(nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Bulk_Delete_v2)
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->insert(vtRandom[nCntr], vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            ErrorCode ec = m_ptrTree->remove(vtRandom[nCntr]);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(vtRandom[nCntr], nValue);

            assert(nCntr == nValue && ec == ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, Bulk_Delete_v3)
    {
        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            ErrorCode ec = m_ptrTree->remove(nCntr);
            assert(ec == ErrorCode::Success);
        }

        for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr--)
        {
            int nValue = 0;
            ErrorCode ec = m_ptrTree->search(nCntr, nValue);

            assert(nCntr == nValue && ec == ErrorCode::KeyDoesNotExist);
        }
    }

    TEST_P(BPlusStore_LRUCache_FileStorage_Suite_1, AllOperations)
    {
        std::vector<int> vtRandom(nTotalRecords);
        std::iota(vtRandom.begin(), vtRandom.end(), 1);
        std::random_device rd; // Obtain a random number from hardware
        std::mt19937 eng(rd()); // Seed the generator
        std::shuffle(vtRandom.begin(), vtRandom.end(), eng);

        for (int nTestCntr = 0; nTestCntr < 2; nTestCntr++)
        {
            for (int nCntr = 0; nCntr < nTotalRecords; nCntr = nCntr + 1)
            {
                ErrorCode ec = m_ptrTree->insert(vtRandom[nCntr], vtRandom[nCntr]);
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                int nValue = 0;
                ErrorCode ec = m_ptrTree->search(vtRandom[nCntr], nValue);

                assert(nValue == vtRandom[nCntr]);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr = nCntr + 2)
            {
                ErrorCode ec = m_ptrTree->remove(vtRandom[nCntr]);

                assert(ec == ErrorCode::Success);
            }
            for (int nCntr = 1; nCntr < nTotalRecords; nCntr = nCntr + 2)
            {
                ErrorCode ec = m_ptrTree->remove(vtRandom[nCntr]);

                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                int nValue = 0;
                ErrorCode ec = m_ptrTree->search(vtRandom[nCntr], nValue);

                assert(ec == ErrorCode::KeyDoesNotExist);
            }
        }

        for (int nTestCntr = 0; nTestCntr < 2; nTestCntr++)
        {
            for (int nCntr = nTotalRecords; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
                assert(ec == ErrorCode::Success);

            }
            for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->insert(nCntr, nCntr);
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                int nValue = 0;
                ErrorCode ec = m_ptrTree->search(nCntr, nValue);

                assert(nValue == nCntr && ec == ErrorCode::Success);
            }

            for (int nCntr = nTotalRecords; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->remove(nCntr);
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = nTotalRecords - 1; nCntr >= 0; nCntr = nCntr - 2)
            {
                ErrorCode ec = m_ptrTree->remove(nCntr);
                assert(ec == ErrorCode::Success);
            }

            for (int nCntr = 0; nCntr < nTotalRecords; nCntr++)
            {
                int nValue = 0;
                ErrorCode ec = m_ptrTree->search(nCntr, nValue);

                assert(ec == ErrorCode::KeyDoesNotExist);
            }
        }
    }

    INSTANTIATE_TEST_CASE_P(
        Insert_Search_Delete,
        BPlusStore_LRUCache_FileStorage_Suite_1,
        ::testing::Values(
            std::make_tuple(3, 1000000, 100, 64, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(4, 1000000, 100, 64, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(5, 1000000, 100, 64, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(6, 1000000, 100, 64, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(7, 1000000, 100, 128, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(8, 1000000, 100, 128, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(15, 1000000, 100, 128, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(16, 1000000, 100, 128, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(32, 1000000, 100, 256, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(64, 1000000, 100, 256, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(128, 1000000, 100, 256, 4ULL * 1024 * 1024 * 1024),
            std::make_tuple(256, 1000000, 100, 256, 10ULL * 1024 * 1024 * 1024),
            std::make_tuple(512, 1000000, 100, 256, 10ULL * 1024 * 1024 * 1024),
            std::make_tuple(1024, 1000000, 100, 256, 10ULL * 1024 * 1024 * 1024),
            std::make_tuple(2048, 1000000, 100, 256, 10ULL * 1024 * 1024 * 1024)
        ));

}
#endif //__TREE_WITH_CACHE__