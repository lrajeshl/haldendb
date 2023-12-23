#include <iostream>

#include "BPlusStore.hpp"
#include "NoCache.hpp"
#include "glog/logging.h"
#include <type_traits>

#include <variant>
#include <typeinfo>

#include "DataNode.hpp"
#include "IndexNode.hpp"

#include <chrono>
#include <cassert>

#include "LRUCache.hpp"
#include "VolatileStorage.hpp"
#include "NoCacheObject.hpp"
#include "LRUCacheObject.hpp"
#include "FileStorage.hpp"
#include "TypeMarshaller.hpp"

#include "TypeUID.h"
#include <iostream>

#include "ObjectFatUID.h"
#include "ObjectUID.h"

//template <typename BPlusStoreType, typename IndexNodeType, typename DataNodeType>
//void insert_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) {
//    for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
//    {
//        ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
//    }
//}
//
//template <typename BPlusStoreType, typename IndexNodeType, typename DataNodeType>
//void reverse_insert_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) {
//    for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
//    {
//        ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
//    }
//}
//
//template <typename BPlusStoreType, typename IndexNodeType, typename DataNodeType>
//void search_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) {
//    for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
//    {
//        int nValue = 0;
//        ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(nCntr, nValue);
//
//        assert(nCntr == nValue);
//    }
//}
//
//template <typename BPlusStoreType, typename IndexNodeType, typename DataNodeType>
//void search_not_found_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) {
//    for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
//    {
//        int nValue = 0;
//        ErrorCode errCode = ptrTree->template search<IndexNodeType, DataNodeType>(nCntr, nValue);
//
//        assert(errCode == ErrorCode::KeyDoesNotExist);
//    }
//}
//
//template <typename BPlusStoreType, typename IndexNodeType, typename DataNodeType>
//void delete_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) {
//    for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
//    {
//        ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(nCntr);
//    }
//}
//
//template <typename BPlusStoreType, typename IndexNodeType, typename DataNodeType>
//void reverse_delete_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) {
//    for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
//    {
//        ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(nCntr);
//    }
//}
//
//template <typename BPlusStoreType, typename IndexNodeType, typename DataNodeType>
//void threaded_test(BPlusStoreType* ptrTree, int degree, int total_entries, int thread_count)
//{
//    vector<std::thread> vtThreads;
//
//    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
//
//    int i = 0;
//    while (i++ < 10) {
//        std::cout << i << std::endl;
//
//        for (int nIdx = 0; nIdx < thread_count; nIdx++)
//        {
//            int nTotal = total_entries / thread_count;
//            vtThreads.push_back(std::thread(insert_concurent<BPlusStoreType, IndexNodeType, DataNodeType>, ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
//        }
//
//        auto it = vtThreads.begin();
//        while (it != vtThreads.end())
//        {
//            (*it).join();
//            it++;
//        }
//
//        vtThreads.clear();
//
//        for (int nIdx = 0; nIdx < thread_count; nIdx++)
//        {
//            int nTotal = total_entries / thread_count;
//            vtThreads.push_back(std::thread(search_concurent<BPlusStoreType, IndexNodeType, DataNodeType>, ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
//        }
//
//        it = vtThreads.begin();
//        while (it != vtThreads.end())
//        {
//            (*it).join();
//            it++;
//        }
//
//        vtThreads.clear();
//
//        for (int nIdx = 0; nIdx < thread_count; nIdx++)
//        {
//            int nTotal = total_entries / thread_count;
//            vtThreads.push_back(std::thread(delete_concurent<BPlusStoreType, IndexNodeType, DataNodeType>, ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
//        }
//
//        it = vtThreads.begin();
//        while (it != vtThreads.end())
//        {
//            (*it).join();
//            it++;
//        }
//
//        vtThreads.clear();
//
//        for (int nIdx = 0; nIdx < thread_count; nIdx++)
//        {
//            int nTotal = total_entries / thread_count;
//            vtThreads.push_back(std::thread(search_not_found_concurent<BPlusStoreType, IndexNodeType, DataNodeType>, ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
//        }
//
//        it = vtThreads.begin();
//        while (it != vtThreads.end())
//        {
//            (*it).join();
//            it++;
//        }
//
//        vtThreads.clear();
//    }
//    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
//    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[µs]" << std::endl;
//    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count() << "[ns]" << std::endl;
//}
//
//template <typename BPlusStoreType, typename IndexNodeType, typename DataNodeType>
//void int_test(BPlusStoreType* ptrTree, int degree, int total_entries)
//{
//    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
//
//    int i = 0;
//    while (i++ < 10) {
//        std::cout << i << std::endl;
//        for (size_t nCntr = 0; nCntr < total_entries; nCntr = nCntr + 2)
//        {
//            ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
//        }
//        for (size_t nCntr = 1; nCntr < total_entries; nCntr = nCntr + 2)
//        {
//            ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
//        }
//
//        for (size_t nCntr = 0; nCntr < total_entries; nCntr++)
//        {
//            int nValue = 0;
//            ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(nCntr, nValue);
//
//            assert(nValue == nCntr);
//        }
//
//        for (size_t nCntr = 0; nCntr < total_entries; nCntr = nCntr + 2)
//        {
//            ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(nCntr);
//        }
//        for (size_t nCntr = 1; nCntr < total_entries; nCntr = nCntr + 2)
//        {
//            ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(nCntr);
//        }
//
//        for (int nCntr = 0; nCntr < total_entries; nCntr++)
//        {
//            int nValue = 0;
//            ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(nCntr, nValue);
//
//            assert(code == ErrorCode::KeyDoesNotExist);
//        }
//    }
//    i = 0;
//    while (i++ < 10) {
//        std::cout << "rev:" << i << std::endl;
//        for (int nCntr = total_entries; nCntr >= 0; nCntr = nCntr - 2)
//        {
//            ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
//        }
//        for (int nCntr = total_entries - 1; nCntr >= 0; nCntr = nCntr - 2)
//        {
//            ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
//        }
//
//        for (int nCntr = 0; nCntr < total_entries; nCntr++)
//        {
//            int nValue = 0;
//            ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(nCntr, nValue);
//
//            assert(nValue == nCntr);
//        }
//
//        for (int nCntr = total_entries; nCntr >= 0; nCntr = nCntr - 2)
//        {
//            ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(nCntr);
//        }
//        for (int nCntr = total_entries - 1; nCntr >= 0; nCntr = nCntr - 2)
//        {
//            ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(nCntr);
//        }
//
//        for (int nCntr = 0; nCntr < total_entries; nCntr++)
//        {
//            int nValue = 0;
//            ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(nCntr, nValue);
//
//            assert(code == ErrorCode::KeyDoesNotExist);
//        }
//
//    }
//    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
//    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[µs]" << std::endl;
//    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count() << "[ns]" << std::endl;
//}
//
//template <typename BPlusStoreType, typename IndexNodeType, typename DataNodeType>
//void string_test(BPlusStoreType* ptrTree, int degree, int total_entries)
//{
//    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
//
//    int i = 0;
//
//    while (i++ < 10) {
//        std::cout << i << std::endl;
//        for (size_t nCntr = 0; nCntr < total_entries; nCntr = nCntr + 2)
//        {
//            ptrTree->template insert<IndexNodeType, DataNodeType>(to_string(nCntr), to_string(nCntr));
//        }
//        for (size_t nCntr = 1; nCntr < total_entries; nCntr = nCntr + 2)
//        {
//            ptrTree->template insert<IndexNodeType, DataNodeType>(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (size_t nCntr = 0; nCntr < total_entries; nCntr++)
//        {
//            string nValue = "";
//            ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(to_string(nCntr), nValue);
//
//            assert(nValue == to_string(nCntr));
//        }
//
//        for (size_t nCntr = 0; nCntr < total_entries; nCntr = nCntr + 2)
//        {
//            ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(to_string(nCntr));
//        }
//        for (size_t nCntr = 1; nCntr < total_entries; nCntr = nCntr + 2)
//        {
//            ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(to_string(nCntr));
//        }
//
//        for (size_t nCntr = 0; nCntr < total_entries; nCntr++)
//        {
//            string nValue = "";
//            ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(to_string(nCntr), nValue);
//
//            assert(code == ErrorCode::KeyDoesNotExist);
//        }
//
//    }
//
//    i = 0;
//    while (i++ < 10) {
//        std::cout << "rev:" << i << std::endl;
//        for (int nCntr = total_entries - 1; nCntr >= 0; nCntr = nCntr - 2)
//        {
//            ptrTree->template insert<IndexNodeType, DataNodeType>(to_string(nCntr), to_string(nCntr));
//        }
//        for (int nCntr = total_entries; nCntr >= 0; nCntr = nCntr - 2)
//        {
//            ptrTree->template insert<IndexNodeType, DataNodeType>(to_string(nCntr), to_string(nCntr));
//        }
//
//        for (int nCntr = 0; nCntr < total_entries; nCntr++)
//        {
//            string nValue = "";
//            ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(to_string(nCntr), nValue);
//
//            assert(nValue == to_string(nCntr));
//        }
//
//        for (int nCntr = total_entries; nCntr >= 0; nCntr = nCntr - 2)
//        {
//            ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(to_string(nCntr));
//        }
//        for (int nCntr = total_entries - 1; nCntr >= 0; nCntr = nCntr - 2)
//        {
//            ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(to_string(nCntr));
//        }
//
//        for (int nCntr = 0; nCntr < total_entries; nCntr++)
//        {
//            string nValue = "";
//            ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(to_string(nCntr), nValue);
//
//            assert(code == ErrorCode::KeyDoesNotExist);
//        }
//
//    }
//
//    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
//    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[µs]" << std::endl;
//    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count() << "[ns]" << std::endl;
//}
//
//void test_for_ints()
//{
//    for (int idx = 3; idx < 20; idx++) {
//        std::cout << "iteration.." << idx << std::endl;
//        {
//            typedef int KeyType;
//            typedef int ValueType;
//            typedef uintptr_t ObjectUIDType;
//
//            typedef IFlushCallback<ObjectUIDType> ICallback;
//
//            typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
//            typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//            typedef BPlusStore<ICallback, KeyType, ValueType, NoCache<ObjectUIDType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;
//            BPlusStoreType ptrTree(idx);
//            ptrTree.template init<DataNodeType>();
//
//            int_test<BPlusStoreType, IndexNodeType, DataNodeType>(&ptrTree, idx, 10000);
//        }
//        {
//            typedef int KeyType;
//            typedef int ValueType;
//            typedef ObjectUID ObjectUIDType;
//
//            typedef IFlushCallback<ObjectUIDType> ICallback;
//
//            typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
//            typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//            typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, VolatileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
//            BPlusStoreType ptrTree(idx, 1000, 100000000);
//            ptrTree.template init<DataNodeType>();
//
//            int_test<BPlusStoreType, IndexNodeType, DataNodeType>(&ptrTree, idx, 10000);
//        }
//        {
//            typedef int KeyType;
//            typedef int ValueType;
//            typedef ObjectFatUID ObjectUIDType;
//
//            typedef IFlushCallback<ObjectUIDType> ICallback;
//
//            typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
//            typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//            typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, FileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
//            BPlusStoreType ptrTree(idx, 10, 1024, 1024 * 1024, "D:\\filestore.hdb");
//            ptrTree.template init<DataNodeType>();
//
//            //int_test<BPlusStoreType, IndexNodeType, DataNodeType>(&ptrTree, idx, 10000);
//        }
//    }
//}
//
//void test_for_string()
//{
//    for (int idx = 3; idx < 20; idx++) {
//        std::cout << "iteration.." << idx << std::endl;
//
//        {
//            typedef string KeyType;
//            typedef string ValueType;
//            typedef uintptr_t ObjectUIDType;
//
//            typedef IFlushCallback<ObjectUIDType> ICallback;
//
//            typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
//            typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//            typedef BPlusStore<ICallback, KeyType, ValueType, NoCache<ObjectUIDType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;
//            BPlusStoreType* ptrTree1 = new BPlusStoreType(idx);
//            ptrTree1->template init<DataNodeType>();
//
//            string_test<BPlusStoreType, IndexNodeType, DataNodeType>(ptrTree1, idx, 10000);
//        }
//        {
//            typedef string KeyType;
//            typedef string ValueType;
//            typedef ObjectUID ObjectUIDType;
//
//            typedef IFlushCallback<ObjectUIDType> ICallback;
//
//            typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
//            typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//            typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, VolatileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
//            BPlusStoreType* ptrTree2 = new BPlusStoreType(idx, 1000, 100000000);
//            ptrTree2->template init<DataNodeType>();
//
//            string_test<BPlusStoreType, IndexNodeType, DataNodeType>(ptrTree2, idx, 10000);
//        }
//        {
//            typedef string KeyType;
//            typedef string ValueType;
//            typedef ObjectFatUID ObjectUIDType;
//
//            typedef IFlushCallback<ObjectUIDType> ICallback;
//
//            typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
//            typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//            typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, FileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
//            BPlusStoreType* ptrTree3 = new BPlusStoreType(idx, 10, 1024, 1024 * 1024, "D:\\filestore.hdb");
//            ptrTree3->template init<DataNodeType>();
//
//            //string_test<BPlusStoreType, IndexNodeType, DataNodeType>(ptrTree3, idx, 10000);
//        }
//    }
//}
//
//void test_for_threaded()
//{
//    for (int idx = 3; idx < 20; idx++) {
//        std::cout << "iteration.." << idx << std::endl;
//        {
//            typedef int KeyType;
//            typedef int ValueType;
//            typedef uintptr_t ObjectUIDType;
//
//            typedef IFlushCallback<ObjectUIDType> ICallback;
//
//            typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
//            typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//            typedef BPlusStore<ICallback, KeyType, ValueType, NoCache<ObjectUIDType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;
//            BPlusStoreType ptrTree(idx);
//            ptrTree.template init<DataNodeType>();
//
//            threaded_test<BPlusStoreType, IndexNodeType, DataNodeType>(&ptrTree, idx, 3 * 10000, 10);
//        }
//        {
//            typedef int KeyType;
//            typedef int ValueType;
//            typedef ObjectUID ObjectUIDType;
//
//            typedef IFlushCallback<ObjectUIDType> ICallback;
//
//            typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
//            typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//            typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, VolatileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
//            BPlusStoreType ptrTree(idx, 1000, 100000000);
//            ptrTree.template init<DataNodeType>();
//
//            threaded_test<BPlusStoreType, IndexNodeType, DataNodeType>(&ptrTree, idx, 3 * 10000, 10);
//        }
//        {
//            typedef int KeyType;
//            typedef int ValueType;
//            typedef ObjectFatUID ObjectUIDType;
//
//            typedef IFlushCallback<ObjectUIDType> ICallback;
//
//            typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
//            typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//            typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, FileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
//            BPlusStoreType ptrTree(idx, 10, 1024, 1024 * 1024, "D:\\filestore.hdb");
//            ptrTree.template init<DataNodeType>();
//
//            //threaded_test<BPlusStoreType, IndexNodeType, DataNodeType>(&ptrTree, idx, 3 * 10000, 10);
//        }
//    }
//}

int main(int argc, char* argv[])
{
    //test_for_ints();
    //test_for_string();
    //test_for_threaded();

    typedef int KeyType;
    typedef int ValueType;

    //typedef uintptr_t ObjectUIDType;
    //typedef IFlushCallback<ObjectUIDType> ICallback;
    //typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
    //typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
    //typedef BPlusStore<ICallback, KeyType, ValueType, NoCache<ObjectUIDType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;
    //BPlusStoreType* ptrTree = new BPlusStoreType(3);

    //typedef ObjectUID ObjectUIDType;
    //typedef IFlushCallback<ObjectUIDType> ICallback;
    //typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
    //typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
    //typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, VolatileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
    //BPlusStoreType* ptrTree = new BPlusStoreType(3, 100000, 100000);

    typedef ObjectFatUID ObjectUIDType;
    typedef IFlushCallback<ObjectUIDType> ICallback;
    typedef DataNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
    typedef IndexNode<KeyType, ValueType, ObjectUIDType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
    typedef BPlusStore<ICallback, KeyType, ValueType, LRUCache<ICallback, FileStorage<ICallback, ObjectUIDType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
    BPlusStoreType* ptrTree = new BPlusStoreType(3, 100, 1024, 1024*1024 * 1024, "D:\\filestore.hdb");


    ptrTree->template init<DataNodeType>();

    for (size_t nCntr = 0; nCntr < 10000; nCntr = nCntr + 2)
    {
        //std::cout << "..........................." << nCntr << std::endl;
        ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
    }
    //std::cout << "\n\n\n\n\n\n\n\n\n..........................." << std::endl;
    for (size_t nCntr = 1; nCntr < 10000; nCntr = nCntr + 2)
    {
        //std::cout << "..........................." << nCntr << std::endl;
        ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
    }
        
    for (size_t nCntr = 0; nCntr < 10000; nCntr++)
    {
        //ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
    }

    for (size_t nCntr = 0; nCntr < 10000; nCntr++)
    {
        int nValue = 0;
        ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(nCntr, nValue);

        assert(nValue == nCntr);
    }

    for (size_t nCntr = 0; nCntr < 10000; nCntr++)
    {
        ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(nCntr);
    }

    for (size_t nCntr = 0; nCntr < 10000; nCntr++)
    {
        int nValue = 0;
        ErrorCode code = ptrTree->template search<IndexNodeType, DataNodeType>(nCntr, nValue);

        assert(code == ErrorCode::KeyDoesNotExist);
    }
    
    char ch = getchar();
    return 0;
}
