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
int main(int argc, char* argv[])
{
    typedef int KeyType;
    typedef int ValueType;
    typedef uintptr_t CacheKeyType;

    typedef DataNode<KeyType, ValueType> LeadNodeType;
    typedef IndexNode<KeyType, ValueType, CacheKeyType> InternalNodeType;

    typedef BPlusStore<KeyType, ValueType, NoCache<CacheKeyType, shared_ptr<LeadNodeType>, shared_ptr<InternalNodeType>>> BPlusStoreType;

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    BPlusStoreType* ptrTree = new BPlusStoreType(3);

    ptrTree->template init<LeadNodeType>();

    int i = 0;

    while (i++ < 10) {
        std::cout << i << std::endl;
        for (size_t nCntr = 0; nCntr < 5000; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }
        for (size_t nCntr = 1; nCntr < 5000; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        ptrTree->template print<InternalNodeType, LeadNodeType>();

        for (size_t nCntr = 0; nCntr < 5000; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            if (nValue != nCntr)
            {
                std::cout << "K: " << nCntr << ", V: " << nValue << std::endl;
            }
        }

        for (size_t nCntr = 0; nCntr < 5000; nCntr = nCntr + 2)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(nCntr);
        }
        for (size_t nCntr = 1; nCntr < 5000; nCntr = nCntr + 2)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(nCntr);
        }
    }
    i = 0;
    while (i++ < 10) {
        std::cout << "rev:" << i << std::endl;
        for (int nCntr = 4999; nCntr >= 0; nCntr = nCntr - 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }
        for (int nCntr = 5000; nCntr >= 0; nCntr = nCntr - 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(nCntr, nCntr);
        }

        for (int nCntr = 0; nCntr < 5000; nCntr++)
        {
            int nValue = 0;
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(nCntr, nValue);

            if (nValue != nCntr)
            {
                std::cout << "K: " << nCntr << ", V: " << nValue << std::endl;
            }
        }

        for (int nCntr = 5000; nCntr >= 0; nCntr = nCntr - 2)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(nCntr);
        }
        for (int nCntr = 4999; nCntr >= 0; nCntr = nCntr - 2)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(nCntr);
        }
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[�s]" << std::endl;
    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count() << "[ns]" << std::endl;
    char ch = getchar();


    /*typedef string KeyType;
    typedef string ValueType;
    typedef uintptr_t CacheKeyType;

    typedef DataNode<KeyType, ValueType> LeadNodeType;
    typedef IndexNode<KeyType, ValueType, CacheKeyType> InternalNodeType;

    typedef BPlusStore<KeyType, ValueType, NoCache<CacheKeyType, shared_ptr<LeadNodeType>, shared_ptr<InternalNodeType>>> BPlusStoreType;

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    BPlusStoreType* ptrTree = new BPlusStoreType(3);

    ptrTree->template init<LeadNodeType>();

    int i = 0;

    while (i++ < 10) {
        std::cout << i << std::endl;
        for (size_t nCntr = 0; nCntr < 50000; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }
        for (size_t nCntr = 1; nCntr < 50000; nCntr = nCntr + 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (size_t nCntr = 0; nCntr < 50000; nCntr++)
        {
            string nValue = "";
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);

            if (nValue != to_string(nCntr))
            {
                std::cout << "K: " << nCntr << ", V: " << nValue << std::endl;
            }
        }

        for (size_t nCntr = 0; nCntr < 50000; nCntr = nCntr + 2)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(to_string(nCntr));
        }
        for (size_t nCntr = 1; nCntr < 50000; nCntr = nCntr + 2)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(to_string(nCntr));
        }
    }
    i = 0;
    while (i++ < 10) {
        std::cout << "rev:" << i << std::endl;
        for (int nCntr = 4999; nCntr >= 0; nCntr = nCntr - 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }
        for (int nCntr = 5000; nCntr >= 0; nCntr = nCntr - 2)
        {
            ptrTree->template insert<InternalNodeType, LeadNodeType>(to_string(nCntr), to_string(nCntr));
        }

        for (int nCntr = 0; nCntr < 5000; nCntr++)
        {
            string nValue = "";
            ErrorCode code = ptrTree->template search<InternalNodeType, LeadNodeType>(to_string(nCntr), nValue);

            if (nValue != to_string(nCntr))
            {
                std::cout << "K: " << nCntr << ", V: " << nValue << std::endl;
            }
        }

        for (int nCntr = 5000; nCntr >= 0; nCntr = nCntr - 2)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(to_string(nCntr));
        }
        for (int nCntr = 4999; nCntr >= 0; nCntr = nCntr - 2)
        {
            ErrorCode code = ptrTree->template remove<InternalNodeType, LeadNodeType>(to_string(nCntr));
        }
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[�s]" << std::endl;
    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count() << "[ns]" << std::endl;
    char ch = getchar();*/
    return 0;
}
