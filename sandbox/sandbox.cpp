#include <iostream>

#include "BTree.h"
#include "BPlusStore.hpp"
#include "NoCache.h"
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

#include "TypeId.h"
//
//typedef int KeyType;
//typedef int ValueType;
//typedef uintptr_t CacheKeyType;
//
//typedef DataNode<KeyType, ValueType, 2000> DataNodeType;
//typedef IndexNode<KeyType, ValueType, CacheKeyType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//typedef BPlusStore<KeyType, ValueType, NoCache<CacheKeyType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;
////typedef BPlusStore<KeyType, ValueType, LRUCache<VolatileStorage, CacheKeyType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>> BPlusStoreType;
////typedef BPlusStore<KeyType, ValueType, LRUCache<FileStorage, CacheKeyType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>> BPlusStoreType;
//
///*typedef BPlusStore<
//            KeyType, 
//            ValueType, 
//            LRUCache<
//                VolatileStorage,    // FileStorage<K, TypesMapper, V, ..., 
//                CacheKeyType,
//                LRUCacheObject, 
//                DataNodeType,
//                IndexNodeType
//            >
//        > BPlusStoreType;
//        */
//
//
//void insert_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) {
//    for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
//    {
//        ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
//    }
//}
//
//void reverse_insert_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) {
//    for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
//    {
//        ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
//    }
//}
//
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
//void delete_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) {
//    for (size_t nCntr = nRangeStart; nCntr < nRangeEnd; nCntr++)
//    {
//        ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(nCntr);
//    }
//}
//
//void reverse_delete_concurent(BPlusStoreType* ptrTree, int nRangeStart, int nRangeEnd) {
//    for (int nCntr = nRangeEnd - 1; nCntr >= nRangeStart; nCntr--)
//    {
//        ErrorCode code = ptrTree->template remove<IndexNodeType, DataNodeType>(nCntr);
//    }
//}
//
//void threaded_test(int degree, int total_entries, int thread_count)
//{
//    vector<std::thread> vtThreads;
//
//    typedef int KeyType;
//    typedef int ValueType;
//    typedef uintptr_t CacheKeyType;
//
//    typedef DataNode<KeyType, ValueType, 2000> DataNodeType;
//    typedef IndexNode<KeyType, ValueType, CacheKeyType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//    typedef BPlusStore<KeyType, ValueType, NoCache<CacheKeyType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;
//    BPlusStoreType* ptrTree = new BPlusStoreType(degree);
//
//    //typedef BPlusStore<KeyType, ValueType, LRUCache<VolatileStorage, CacheKeyType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>> BPlusStoreType;
//    //BPlusStoreType* ptrTree = new BPlusStoreType(degree, 10000, 10000000);
//
//    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
//
//    ptrTree->template init<DataNodeType>();
//
//    int i = 0;
//    while (i++ < 10) {
//        std::cout << i << std::endl;
//
//        for (int nIdx = 0; nIdx < thread_count; nIdx++)
//        {
//            int nTotal = total_entries / thread_count;
//            vtThreads.push_back(std::thread(insert_concurent, ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
//            vtThreads.push_back(std::thread(search_concurent, ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
//            vtThreads.push_back(std::thread(delete_concurent, ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
//            vtThreads.push_back(std::thread(search_not_found_concurent, ptrTree, nIdx * nTotal, nIdx * nTotal + nTotal));
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
//void int_test(int degree, int total_entries)
//{
//    typedef int KeyType;
//    typedef int ValueType;
//    typedef uintptr_t CacheKeyType;
//
//    typedef DataNode<KeyType, ValueType, __COUNTER__> DataNodeType;
//    typedef IndexNode<KeyType, ValueType, CacheKeyType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//    typedef BPlusStore<KeyType, ValueType, NoCache<CacheKeyType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;
//    BPlusStoreType* ptrTree = new BPlusStoreType(degree);
//
//    //typedef BPlusStore<KeyType, ValueType, LRUCache<VolatileStorage, CacheKeyType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>> BPlusStoreType;
//    //BPlusStoreType* ptrTree = new BPlusStoreType(degree, 10, 10000000);
//
//    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
//
//    ptrTree->template init<DataNodeType>();
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
//void string_test(int degree, int total_entries)
//{
//    typedef string KeyType;
//    typedef string ValueType;
//    typedef uintptr_t CacheKeyType;
//
//    typedef DataNode<KeyType, ValueType, __COUNTER__> DataNodeType;
//    typedef IndexNode<KeyType, ValueType, CacheKeyType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;
//
//    typedef BPlusStore<KeyType, ValueType, NoCache<CacheKeyType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;
//    BPlusStoreType* ptrTree = new BPlusStoreType(degree);
//
//    //typedef BPlusStore<KeyType, ValueType, LRUCache<VolatileStorage, CacheKeyType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>> BPlusStoreType;
//    //BPlusStoreType* ptrTree = new BPlusStoreType(degree, 10, 10000000);
//
//    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
//
//    ptrTree->template init<DataNodeType>();
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

#include <iostream>

// Sample types for the variant
struct Integer {
    int value;
};

struct Double {
    double value;
};

struct String {
    std::string value;
};

// Recursive helper to wrap each type with std::shared_ptr
template<typename... Ts>
struct VariantWithShared {
    using type = std::variant<std::shared_ptr<Ts>...>;
};

// Helper function to create shared_ptr for each type
template<typename T>
std::shared_ptr<T> make_shared_variant(const T& value) {
    return std::make_shared<T>(value);
}

using MyVariantType = typename VariantWithShared<Integer, Double, String>::type;
//using MyVariantType_ = typename VariantWithShared<int, Double, String>::type;

// Create an instance of the variant with shared_ptr

// Template function with a template lambda for folding
template<typename... Args>
auto sum(Args... args) {
    // Define a lambda inside the function for folding
    auto foldLambda = [](auto... values) {
        return (values + ...);
    };

    // Use the lambda to fold the arguments
    return foldLambda(args...);
}

#include "CacheObjectKey.h"
#include "CacheObjectFatKey.h"

int main(int argc, char* argv[])
{
    VariantWithShared<Integer, Double, String>::type _myVariant = make_shared<Integer>();  // Wrapped in std::shared_ptr automatically
        // Example usage
    int result = sum(1, 2, 3, 4, 5);
    std::cout << "Sum: " << result << std::endl;

    // You can use the sum function with different types
    double doubleResult = sum(1.1, 2.2, 3.3, 4.4, 5.5);
    std::cout << "Sum (double): " << doubleResult << std::endl;


    typedef int KeyType;
    typedef int ValueType;
    //typedef uintptr_t CacheKeyType;
    typedef CacheObjectFatKey CacheKeyType;

    typedef DataNode<KeyType, ValueType, TYPE_UID::DATA_NODE_INT_INT> DataNodeType;
    typedef IndexNode<KeyType, ValueType, CacheKeyType, TYPE_UID::INDEX_NODE_INT_INT> IndexNodeType;

    typedef IFlushCallback<CacheKeyType> ICallback;
    typedef BPlusStore<KeyType, ValueType, NoCache<CacheKeyType, NoCacheObject, DataNodeType, IndexNodeType>> BPlusStoreType;
    BPlusStoreType* ptrTree = new BPlusStoreType(3);

    //typedef BPlusStore<string, string, LRUCache<FileStorage, CacheKeyType, LRUCacheObject, TypeMarshaller, DataNode<string, string, __COUNTER__>, IndexNode<string, string, CacheKeyType>>> BPlusStoreType2;
    //BPlusStoreType2* ptrTree2 = new BPlusStoreType2(3, 10, 1, 1, "");

    //LRUCache<FileStorage, CacheKeyType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>* _l = new LRUCache<FileStorage, CacheKeyType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>(nullptr, 10, 1024, 1024 * 1024, "D:\\filestore.hdb");
    //typedef BPlusStore<KeyType, ValueType, LRUCache<ICallback, FileStorage<ICallback, CacheKeyType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
    //BPlusStoreType* ptrTree = new BPlusStoreType(3, 10, 1024, 1024 * 1024, "D:\\filestore.hdb");

    //typedef BPlusStore<KeyType, ValueType, LRUCache<ICallback, VolatileStorage<CacheKeyType, LRUCacheObject, TypeMarshaller, DataNodeType, IndexNodeType>>> BPlusStoreType;
    //BPlusStoreType* ptrTree = new BPlusStoreType(3, 100000, 100000);

    ptrTree->template init<DataNodeType>();

    for (size_t nCntr = 0; nCntr < 10000; nCntr++)
    {
        ptrTree->template insert<IndexNodeType, DataNodeType>(nCntr, nCntr);
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
    

    //for (int idx = 3; idx < 20; idx++) {
    //    std::cout << "iteration.." << idx << std::endl;
    //    int_test(idx, 10000);
    //    string_test(idx, 10000);
    //    threaded_test(idx, 10000, 10);
    //}

    char ch = getchar();
    return 0;
}
