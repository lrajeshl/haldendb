#pragma once
#include <memory>
#include <iostream>
#include <stack>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <syncstream>
#include <thread>
#include <cmath>
#include <exception>
#include <variant>
#include <unordered_map>
#include "CacheErrorCodes.h"
#include "ErrorCodes.h"
#include "VariadicNthType.h"
#include <tuple>
#include <vector>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <algorithm>

using namespace std::chrono_literals;

#ifdef __TREE_WITH_CACHE__
template <typename ICallback, typename KeyType, typename ValueType, typename CacheType>
class BPlusStore : public ICallback
#else // !__TREE_WITH_CACHE__
template <typename KeyType, typename ValueType, typename CacheType>
class BPlusStore
#endif __TREE_WITH_CACHE__
{
    typedef CacheType::ObjectUIDType ObjectUIDType;
    typedef CacheType::ObjectType ObjectType;
    typedef CacheType::ObjectTypePtr ObjectTypePtr;

    using DataNodeType = typename std::tuple_element<0, typename ObjectType::ValueCoreTypesTuple>::type;
    using IndexNodeType = typename std::tuple_element<1, typename ObjectType::ValueCoreTypesTuple>::type;

private:
    uint32_t m_nDegree;
    std::shared_ptr<CacheType> m_ptrCache;
    std::optional<ObjectUIDType> m_uidRootNode;

#ifdef __CONCURRENT__
    mutable std::shared_mutex m_mutex;
#endif __CONCURRENT__

public:
    ~BPlusStore()
    {
    }

    template<typename... CacheArgs>
    BPlusStore(uint32_t nDegree, CacheArgs... args)
        : m_nDegree(nDegree)
        , m_uidRootNode(std::nullopt)
    {
        m_ptrCache = std::make_shared<CacheType>(args...);
    }

    template <typename DefaultNodeType>
    void init()
    {
#ifdef __TREE_WITH_CACHE__
        m_ptrCache->init(this);
#endif __TREE_WITH_CACHE__

        m_ptrCache->template createObjectOfType<DefaultNodeType>(m_uidRootNode);
    }

    ErrorCode insert(const KeyType& key, const ValueType& value, bool print = false)
    {
        ErrorCode ecResult = ErrorCode::Error;

#ifdef __TRACK_CACHE_FOOTPRINT__
        int32_t nMemoryFootprint = 0;
#endif __TRACK_CACHE_FOOTPRINT__

#ifdef __TREE_WITH_CACHE__
        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtAccessedNodes;
#endif __TREE_WITH_CACHE__

        ObjectUIDType uidLastNode, uidCurrentNode;  // TODO: make Optional!
        ObjectTypePtr ptrLastNode = nullptr, ptrCurrentNode = nullptr;

        KeyType pivotKey;
        std::optional<ObjectUIDType> uidRHSChildNode, uidLHSChildNode;
        ObjectTypePtr ptrRHSChildNode = nullptr, ptrLHSChildNode = nullptr;

        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtNodes;

#ifdef __CONCURRENT__
        std::vector<std::unique_lock<std::shared_mutex>> vtLocks;
        vtLocks.emplace_back(std::unique_lock<std::shared_mutex>(m_mutex));
#endif __CONCURRENT__

        uidCurrentNode = m_uidRootNode.value();
        do
        {
#ifdef __TREE_WITH_CACHE__
            std::optional<ObjectUIDType> uidUpdated = std::nullopt;
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode, uidUpdated);
#else __TREE_WITH_CACHE__
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode);
#endif __TREE_WITH_CACHE__

            if (ptrCurrentNode == nullptr)
            {
                throw new std::logic_error("should not occur!");
            }
#ifdef __CONCURRENT__
            vtLocks.emplace_back(std::unique_lock<std::shared_mutex>(ptrCurrentNode->getMutex()));
#endif __CONCURRENT__

#ifdef __TREE_WITH_CACHE__
            if (uidUpdated != std::nullopt)
            {
                if (ptrLastNode != nullptr)
                {
                    std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrLastNode->getInnerData());

#ifdef __TRACK_CACHE_FOOTPRINT__
                    ptrIndexNode->template updateChildUID<ObjectType>(ptrCurrentNode, uidCurrentNode, *uidUpdated, nMemoryFootprint);
#else __TRACK_CACHE_FOOTPRINT__
                    ptrIndexNode->template updateChildUID<ObjectType>(ptrCurrentNode, uidCurrentNode, *uidUpdated);
#endif __TRACK_CACHE_FOOTPRINT__
                    //ptrIndexNode->updateChildUID(uidCurrentNode, *uidUpdated);
                    ptrLastNode->setDirtyFlag(true);
                }
                else
                {
                    assert(uidCurrentNode == *m_uidRootNode);
                    m_uidRootNode = uidUpdated;
                }

                uidCurrentNode = *uidUpdated;
            }

            vtAccessedNodes.push_back(std::make_pair(uidCurrentNode, ptrCurrentNode));
#endif __TREE_WITH_CACHE__

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(ptrCurrentNode->getInnerData()))
            {
                vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidCurrentNode, ptrCurrentNode));

                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrCurrentNode->getInnerData());

                if (!ptrIndexNode->canTriggerSplit(m_nDegree))
                {
#ifdef __CONCURRENT__
                    // Although lock to the last node is enough. 
                    // However, the preceeding one is maintainig for the root node so that "m_uidRootNode" can be updated safely if the root node is needed to be deleted.
                    vtLocks.erase(vtLocks.begin(), vtLocks.end() - 2);
#endif __CONCURRENT__
                    vtNodes.erase(vtNodes.begin(), vtNodes.end() - 1);
                }

                uidLastNode = uidCurrentNode;
                ptrLastNode = ptrCurrentNode;

                uidCurrentNode = ptrIndexNode->getChild(key);
            }
            else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(ptrCurrentNode->getInnerData()))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(ptrCurrentNode->getInnerData());

#ifdef __TRACK_CACHE_FOOTPRINT__
                if (ptrDataNode->insert(key, value, nMemoryFootprint) != ErrorCode::Success)
#else __TRACK_CACHE_FOOTPRINT__
                if (ptrDataNode->insert(key, value) != ErrorCode::Success)
#endif __TRACK_CACHE_FOOTPRINT__
                {
#ifdef __CONCURRENT__
                    vtLocks.clear();
#endif __CONCURRENT__
                    vtNodes.clear();

                    ecResult = ErrorCode::InsertFailed;
                    break;
                }
                else
                {
                    ecResult = ErrorCode::Success;
                }

#ifdef __TREE_WITH_CACHE__
                ptrCurrentNode->setDirtyFlag(true);
#endif __TREE_WITH_CACHE__

                if (ptrDataNode->requireSplit(m_nDegree))
                {
#ifdef __TRACK_CACHE_FOOTPRINT__
                    ErrorCode errCode = ptrDataNode->template split<CacheType, ObjectTypePtr>(m_ptrCache, uidRHSChildNode, ptrRHSChildNode, pivotKey, nMemoryFootprint);
#else __TRACK_CACHE_FOOTPRINT__
                    ErrorCode errCode = ptrDataNode->template split<CacheType, ObjectTypePtr>(m_ptrCache, uidRHSChildNode, ptrRHSChildNode, pivotKey);
#endif __TRACK_CACHE_FOOTPRINT__

                    if (errCode != ErrorCode::Success)
                    {
                        throw new std::logic_error(".....");
                    }

                    uidLHSChildNode = uidCurrentNode;
                    ptrLHSChildNode = ptrCurrentNode;

#ifdef __TREE_WITH_CACHE__
                    bool bTest = false;
                    for (auto itCurrent = vtAccessedNodes.cbegin(), itEnd = vtAccessedNodes.cend(); itCurrent != itEnd; itCurrent++)
                    {
                        if ((*itCurrent).first == uidCurrentNode)
                        {
                            bTest = true;
                            vtAccessedNodes.insert(itCurrent + 1, std::make_pair(*uidRHSChildNode, nullptr));
                            break;
                        }
                    }

                    if (!bTest)
                    {
                        throw new std::logic_error(".....");
                    }
#endif __TREE_WITH_CACHE__
                }
                else
                {
#ifdef __CONCURRENT__
                    vtLocks.clear();
#endif __CONCURRENT__
                    vtNodes.clear();
                }

                break;
            }
        } while (true);

        while (vtNodes.size() > 0)
        {
            uidCurrentNode = vtNodes.back().first;
            ptrCurrentNode = vtNodes.back().second;

            std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrCurrentNode->getInnerData());

#ifdef __TRACK_CACHE_FOOTPRINT__
            if (ptrIndexNode->insert(pivotKey, *uidRHSChildNode, nMemoryFootprint) != ErrorCode::Success)
#else __TRACK_CACHE_FOOTPRINT__
            if (ptrIndexNode->insert(pivotKey, *uidRHSChildNode) != ErrorCode::Success)
#endif __TRACK_CACHE_FOOTPRINT__
            {
                // TODO: Should update be performed on cloned objects first?
                throw new std::logic_error("should not occur!"); // for the time being!
            }

#ifdef __TREE_WITH_CACHE__
            ptrCurrentNode->setDirtyFlag(true);
#endif __TREE_WITH_CACHE__

            uidRHSChildNode = std::nullopt;
            ptrRHSChildNode = nullptr;

            if (ptrIndexNode->requireSplit(m_nDegree))
            {
#ifdef __TRACK_CACHE_FOOTPRINT__
                ErrorCode errCode = ptrIndexNode->template split<CacheType>(m_ptrCache, uidRHSChildNode, ptrRHSChildNode, pivotKey, nMemoryFootprint);
#else __TRACK_CACHE_FOOTPRINT__
                ErrorCode errCode = ptrIndexNode->template split<CacheType>(m_ptrCache, uidRHSChildNode, ptrRHSChildNode, pivotKey);
#endif __TRACK_CACHE_FOOTPRINT__

                if (errCode != ErrorCode::Success)
                {
                    // TODO: Should update be performed on cloned objects first?
                    throw new std::logic_error("should not occur!"); // for the time being!
                }

#ifdef __TREE_WITH_CACHE__
                bool test = false;
                for (auto itCurrent = vtAccessedNodes.cbegin(), itEnd = vtAccessedNodes.cend(); itCurrent != itEnd; itCurrent++)
                {
                    if ((*itCurrent).first == uidCurrentNode)
                    {
                        test = true;
                        vtAccessedNodes.insert(itCurrent + 1, std::make_pair(*uidRHSChildNode, nullptr));
                        break;
                    }
                }

                if (!test)
                {
                    throw new std::logic_error("should not occur!");
                }
#endif __TREE_WITH_CACHE__
            }

            uidLHSChildNode = uidCurrentNode;
            ptrLHSChildNode = ptrCurrentNode;

#ifdef __CONCURRENT__
            vtLocks.pop_back();
#endif __CONCURRENT__

            vtNodes.pop_back();
        }

        if (uidCurrentNode == m_uidRootNode && ptrLHSChildNode != nullptr && ptrRHSChildNode != nullptr)
        {
            m_uidRootNode = std::nullopt;
            m_ptrCache->template createObjectOfType<IndexNodeType>(m_uidRootNode, pivotKey, *uidLHSChildNode, *uidRHSChildNode);

#ifdef __TREE_WITH_CACHE__
            ptrCurrentNode->setDirtyFlag(true);

            bool bTest = false;
            for (auto itCurrent = vtAccessedNodes.cbegin(), itEnd = vtAccessedNodes.cend(); itCurrent != itEnd; itCurrent++)
            {
                if ((*itCurrent).first == uidCurrentNode)
                {
                    bTest = true;
                    vtAccessedNodes.insert(itCurrent + 1, std::make_pair(*uidRHSChildNode, nullptr));
                    break;
                }
            }

            if (!bTest)
            {
                throw new std::logic_error("should not occur!");
            }
        }
        //else if (uidCurrentNode != m_uidRootNode && ptrRHSChildNode != nullptr)
        //{
        //    throw new std::logic_error("should not occur!");
        //    
        //    bool bTest = false;
        //    for (auto itCurrent = vtAccessedNodes.cbegin(), itEnd = vtAccessedNodes.cend(); itCurrent != itEnd; itCurrent++)
        //    {
        //        if ((*itCurrent).first == uidCurrentNode)
        //        {
        //            bTest = true;
        //            vtAccessedNodes.insert(itCurrent + 1, std::make_pair(*uidRHSChildNode, nullptr));
        //            break;
        //        }
        //    }

        //    if (!bTest)
        //    {
        //        throw new std::logic_error("should not occur!");
        //    }
        //}

        m_ptrCache->reorder(vtAccessedNodes);
        vtAccessedNodes.clear();
#else __TREE_WITH_CACHE__
        }
#endif __TREE_WITH_CACHE__

        vtLocks.clear();

#ifdef __TRACK_CACHE_FOOTPRINT__
        if (nMemoryFootprint != 0)
        {
            m_ptrCache->updateMemoryFootprint(nMemoryFootprint);
        }
#endif __TRACK_CACHE_FOOTPRINT__
        return ecResult;
    }

    ErrorCode search(const KeyType& key, ValueType& value)
    {
        ErrorCode ecResult = ErrorCode::Error;

#ifdef __TRACK_CACHE_FOOTPRINT__
        int32_t nMemoryFootprint = 0;
#endif __TRACK_CACHE_FOOTPRINT__

#ifdef __TREE_WITH_CACHE__
        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtAccessedNodes;
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
        std::vector<std::unique_lock<std::shared_mutex>> vtLocks;
        vtLocks.emplace_back(std::unique_lock<std::shared_mutex>(m_mutex));
#endif __CONCURRENT__

        ObjectTypePtr ptrCurrentNode = nullptr;
        ObjectUIDType uidCurrentNode = *m_uidRootNode;

        do
        {
#ifdef __TREE_WITH_CACHE__
            std::optional<ObjectUIDType> uidUpdated = std::nullopt;
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode, uidUpdated);
#else __TREE_WITH_CACHE__
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode);
#endif __TREE_WITH_CACHE__

            if (ptrCurrentNode == nullptr)
            {
                throw new std::logic_error("Cache/Storage does not contain the requested object.");
            }

#ifdef __CONCURRENT__
            vtLocks.emplace_back(std::unique_lock<std::shared_mutex>(ptrCurrentNode->getMutex()));
#endif __CONCURRENT__

#ifdef __TREE_WITH_CACHE__
            if (uidUpdated != std::nullopt)
            {
                ObjectTypePtr ptrLastNode = vtAccessedNodes.size() > 0 ? vtAccessedNodes[vtAccessedNodes.size() - 1].second : nullptr;
                if (ptrLastNode != nullptr)
                {
                    std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrLastNode->getInnerData());
 
#ifdef __TRACK_CACHE_FOOTPRINT__
                    ptrIndexNode->template updateChildUID<ObjectType>(ptrCurrentNode, uidCurrentNode, *uidUpdated, nMemoryFootprint);
#else __TRACK_CACHE_FOOTPRINT__
                    ptrIndexNode->template updateChildUID<ObjectType>(ptrCurrentNode, uidCurrentNode, *uidUpdated);
#endif __TRACK_CACHE_FOOTPRINT__
                    //ptrIndexNode->updateChildUID(uidCurrentNode, *uidUpdated);

                    ptrLastNode->setDirtyFlag(true);
                }
                else
                {
                    assert(uidCurrentNode == *m_uidRootNode);
                    m_uidRootNode = uidUpdated;
                }

                uidCurrentNode = *uidUpdated;
            }
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
            vtLocks.erase(vtLocks.begin());
#endif __CONCURRENT__

#ifdef __TREE_WITH_CACHE__
            vtAccessedNodes.push_back(std::make_pair(uidCurrentNode, ptrCurrentNode));
#endif __TREE_WITH_CACHE__

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(ptrCurrentNode->getInnerData()))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrCurrentNode->getInnerData());

                uidCurrentNode = ptrIndexNode->getChild(key);
            }
            else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(ptrCurrentNode->getInnerData()))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(ptrCurrentNode->getInnerData());

                ecResult = ptrDataNode->getValue(key, value);

                break;
            }

        } while (true);

#ifdef __TREE_WITH_CACHE__
        m_ptrCache->reorder(vtAccessedNodes);
        vtAccessedNodes.clear();
#endif __TREE_WITH_CACHE__

        vtLocks.clear();

#ifdef __TRACK_CACHE_FOOTPRINT__
        if (nMemoryFootprint != 0)
        {
            m_ptrCache->updateMemoryFootprint(nMemoryFootprint);
        }
#endif __TRACK_CACHE_FOOTPRINT__

        return ecResult;
    }

    ErrorCode remove(const KeyType& key)
    {
        ErrorCode ecResult = ErrorCode::Success;

#ifdef __TRACK_CACHE_FOOTPRINT__
        int32_t nMemoryFootprint = 0;
#endif __TRACK_CACHE_FOOTPRINT__

        ObjectUIDType uidLastNode, uidCurrentNode;
        ObjectTypePtr ptrLastNode = nullptr, ptrCurrentNode = nullptr;

        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtNodes;

#ifdef __TREE_WITH_CACHE__
        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtAccessedNodes;
#endif __TREE_WITH_CACHE__

#ifdef __CONCURRENT__
        std::vector<std::unique_lock<std::shared_mutex>> vtLocks;
        vtLocks.emplace_back(std::unique_lock<std::shared_mutex>(m_mutex));
#endif __CONCURRENT__

        uidCurrentNode = m_uidRootNode.value();

        //vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidLastNode, ptrLastNode));

        do
        {
#ifdef __TREE_WITH_CACHE__
            std::optional<ObjectUIDType> uidUpdated = std::nullopt;
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode, uidUpdated);
#else __TREE_WITH_CACHE__
            m_ptrCache->getObject(uidCurrentNode, ptrCurrentNode);
#endif __TREE_WITH_CACHE__

            if (ptrCurrentNode == nullptr)
            {
                throw new std::logic_error("Cache/Storage does not contain the requested object.");
            }

#ifdef __CONCURRENT__
            vtLocks.emplace_back(std::unique_lock<std::shared_mutex>(ptrCurrentNode->getMutex()));
#endif __CONCURRENT__

#ifdef __TREE_WITH_CACHE__
            if (uidUpdated != std::nullopt)
            {
                if (ptrLastNode != nullptr)
                {
                    std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrLastNode->getInnerData());

#ifdef __TRACK_CACHE_FOOTPRINT__
                    ptrIndexNode->template updateChildUID<ObjectType>(ptrCurrentNode, uidCurrentNode, *uidUpdated, nMemoryFootprint);
#else __TRACK_CACHE_FOOTPRINT__
                    ptrIndexNode->template updateChildUID<ObjectType>(ptrCurrentNode, uidCurrentNode, *uidUpdated);
#endif __TRACK_CACHE_FOOTPRINT__

                    ptrLastNode->setDirtyFlag(true);
                }
                else
                {
                    assert(uidCurrentNode == *m_uidRootNode);
                    m_uidRootNode = uidUpdated;
                }

                uidCurrentNode = *uidUpdated;
            }
#endif __TREE_WITH_CACHE__

            vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidCurrentNode, ptrCurrentNode));

#ifdef __TREE_WITH_CACHE__
            vtAccessedNodes.push_back(std::make_pair(uidCurrentNode, ptrCurrentNode));
#endif __TREE_WITH_CACHE__

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(ptrCurrentNode->getInnerData()))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrCurrentNode->getInnerData());

                if (!ptrIndexNode->canTriggerMerge(m_nDegree))
                {
#ifdef __CONCURRENT__
                    // Although lock to the last node is enough. 
                    // However, the preceeding one is maintainig for the root node so that "m_uidRootNode" can be updated safely if the root node is needed to be deleted.
                    vtLocks.erase(vtLocks.begin(), vtLocks.end() - 2);
#endif __CONCURRENT__
                    vtNodes.erase(vtNodes.begin(), vtNodes.end() - 1);
                }

                uidLastNode = uidCurrentNode;
                ptrLastNode = ptrCurrentNode;

                uidCurrentNode = ptrIndexNode->getChild(key);
            }
            else // if (std::holds_alternative<std::shared_ptr<DataNodeType>>(ptrCurrentNode->getInnerData()))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(ptrCurrentNode->getInnerData());

#ifdef __TRACK_CACHE_FOOTPRINT__
                if (ptrDataNode->remove(key, nMemoryFootprint) == ErrorCode::KeyDoesNotExist)
#else __TRACK_CACHE_FOOTPRINT__
                if (ptrDataNode->remove(key) == ErrorCode::KeyDoesNotExist)
#endif __TRACK_CACHE_FOOTPRINT__
                {
                    ecResult = ErrorCode::KeyDoesNotExist;

#ifdef __CONCURRENT__
                    vtLocks.pop_back();
#endif __CONCURRENT__
                    vtNodes.pop_back();

                    break;
                }

#ifdef __TREE_WITH_CACHE__
                ptrCurrentNode->setDirtyFlag( true);
#endif __TREE_WITH_CACHE__

                if (ptrDataNode->requireMerge(m_nDegree))
                {
                    if (ptrLastNode != nullptr) 
                    {
                        std::optional<ObjectUIDType> uidToDelete = std::nullopt;

                        std::shared_ptr<IndexNodeType> ptrParentNode = std::get<std::shared_ptr<IndexNodeType>>(ptrLastNode->getInnerData());
#ifdef __TREE_WITH_CACHE__
                        std::optional<ObjectUIDType> uidAffectedNode = std::nullopt;
                        ObjectTypePtr ptrAffectedNode = nullptr;

#ifdef __TRACK_CACHE_FOOTPRINT__
                        ptrParentNode->template rebalanceDataNode<CacheType>(m_ptrCache, uidCurrentNode, ptrDataNode, key, m_nDegree, uidToDelete, uidAffectedNode, ptrAffectedNode, nMemoryFootprint);
#else __TRACK_CACHE_FOOTPRINT__
                        ptrParentNode->template rebalanceDataNode<CacheType>(m_ptrCache, uidCurrentNode, ptrDataNode, key, m_nDegree, uidToDelete, uidAffectedNode, ptrAffectedNode);
#endif __TRACK_CACHE_FOOTPRINT__



#else __TREE_WITH_CACHE__
                        ptrParentNode->template rebalanceDataNode<CacheType>(m_ptrCache, uidCurrentNode, ptrDataNode, key, m_nDegree, uidToDelete);
#endif __TREE_WITH_CACHE__

#ifdef __TREE_WITH_CACHE__
                        ptrLastNode->setDirtyFlag(true);
                        ptrCurrentNode->setDirtyFlag(true);

                        bool bTest = false;
                        for (auto itCurrent = vtAccessedNodes.cbegin(), itEnd = vtAccessedNodes.cend(); itCurrent != itEnd; itCurrent++)
                        {
                            if ((*itCurrent).first == uidLastNode)
                            {
                                bTest = true;
                                vtAccessedNodes.insert(itCurrent + 1, std::make_pair(*uidAffectedNode, nullptr));
                                break;
                            }
                        }

                        if (!bTest)
                        {
                            throw new std::logic_error("should not occur!");
                        }
#endif __TREE_WITH_CACHE__


#ifdef __CONCURRENT__
                        vtLocks.pop_back();
#endif __CONCURRENT__
                        vtNodes.pop_back();

                        if (uidToDelete)
                        {
                            m_ptrCache->remove(*uidToDelete);
                        }
                    }
                }
                else
                {
#ifdef __CONCURRENT__
                    vtLocks.clear();
#endif __CONCURRENT__
                    vtNodes.clear();
                }

                break;
            }
        } while (true);

        ObjectUIDType uidChildNode;
        ObjectTypePtr ptrChildNode = nullptr;

        if (vtNodes.size() > 0)
        {
            uidChildNode = vtNodes.back().first;
            ptrChildNode = vtNodes.back().second;

            vtNodes.pop_back();

            while (vtNodes.size() > 0)
            {
                //uidCurrentNode = vtNodes.back().first; 
                ptrCurrentNode = vtNodes.back().second;

                bool bReleaseLock = true;
                std::optional<ObjectUIDType> uidToDelete = std::nullopt;

                std::shared_ptr<IndexNodeType> ptrParentIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrCurrentNode->getInnerData());

                std::shared_ptr<IndexNodeType> ptrChildIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrChildNode->getInnerData());

                if (ptrChildIndexNode->requireMerge(m_nDegree))
                {
#ifdef __TREE_WITH_CACHE__
                    std::optional<ObjectUIDType> uidAffectedNode = std::nullopt;
                    ObjectTypePtr ptrAffectedNode = nullptr;

#ifdef __TRACK_CACHE_FOOTPRINT__
                    ptrParentIndexNode->template rebalanceIndexNode<CacheType>(m_ptrCache, uidChildNode, ptrChildIndexNode, key, m_nDegree, uidToDelete, uidAffectedNode, ptrAffectedNode, nMemoryFootprint);
#else __TRACK_CACHE_FOOTPRINT__
                    ptrParentIndexNode->template rebalanceIndexNode<CacheType>(m_ptrCache, uidChildNode, ptrChildIndexNode, key, m_nDegree, uidToDelete, uidAffectedNode, ptrAffectedNode);
#endif __TRACK_CACHE_FOOTPRINT__


#else __TREE_WITH_CACHE__
                    ptrParentIndexNode->template rebalanceIndexNode<CacheType>(m_ptrCache, uidChildNode, ptrChildIndexNode, key, m_nDegree, uidToDelete);
#endif __TREE_WITH_CACHE__

#ifdef __TREE_WITH_CACHE__
                    ptrCurrentNode->setDirtyFlag(true);
                    ptrChildNode->setDirtyFlag(true);

                    bool bTest = false;
                    for (auto itCurrent = vtAccessedNodes.cbegin(), itEnd = vtAccessedNodes.cend(); itCurrent != itEnd; itCurrent++)
                    {
                        if ((*itCurrent).first == uidCurrentNode)
                        {
                            bTest = true;
                            vtAccessedNodes.insert(itCurrent + 1, std::make_pair(*uidAffectedNode, nullptr));
                            break;
                        }
                    }

                    if (!bTest)
                    {
                        throw new std::logic_error("should not occur!");
                    }
#endif __TREE_WITH_CACHE__

                    if (uidToDelete)
                    {
#ifdef __CONCURRENT__
                        vtLocks.pop_back();
                        bReleaseLock = false;
#endif __CONCURRENT__
                        m_ptrCache->remove(*uidToDelete);
                    }
                }

#ifdef __CONCURRENT__
                if (bReleaseLock)
                {
                    vtLocks.pop_back();
                }
#endif __CONCURRENT__

                uidChildNode = vtNodes.back().first;
                ptrChildNode = vtNodes.back().second;
                vtNodes.pop_back();
            }

            if (ptrChildNode != nullptr && m_uidRootNode == uidChildNode)
            {
                if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(ptrChildNode->getInnerData()))
                {
                    std::shared_ptr<IndexNodeType> ptrInnerNode = std::get<std::shared_ptr<IndexNodeType>>(ptrChildNode->getInnerData());
                    if (ptrInnerNode->getKeysCount() == 0)
                    {
#ifdef __CONCURRENT__
                        vtLocks.pop_back();
#endif __CONCURRENT__

                        ObjectUIDType uidNewRootNode = ptrInnerNode->getChildAt(0);
                        m_ptrCache->remove(uidChildNode);
                        m_uidRootNode = uidNewRootNode;

#ifdef __TREE_WITH_CACHE__
                        // ptrChildNode->setDirtyFlag(true); Not needed!
#endif __TREE_WITH_CACHE__
                    }
                }
            }
        }

#ifdef __TREE_WITH_CACHE__
        m_ptrCache->reorder(vtAccessedNodes, false);
        vtAccessedNodes.clear();
#endif __TREE_WITH_CACHE__

        vtLocks.clear();

#ifdef __TRACK_CACHE_FOOTPRINT__
        if (nMemoryFootprint != 0)
        {
            m_ptrCache->updateMemoryFootprint(nMemoryFootprint);
        }
#endif __TRACK_CACHE_FOOTPRINT__

        return ecResult;
    }

    void print(std::ofstream & os)
    {
        int nSpace = 7;

        std::string prefix;

        os << prefix << "|" << std::endl;
        os << prefix << "|" << std::string(nSpace, '-').c_str() << "(root)";

        os << std::endl;

        ObjectTypePtr ptrRootNode = nullptr;

#ifdef __TREE_WITH_CACHE__
        std::optional<ObjectUIDType> uidUpdated = std::nullopt;
        m_ptrCache->getObject(m_uidRootNode.value(), ptrRootNode, uidUpdated);

        if (uidUpdated != std::nullopt)
        {
            m_uidRootNode = uidUpdated;
        }
#else __TREE_WITH_CACHE__
        m_ptrCache->getObject(m_uidRootNode.value(), ptrRootNode);
#endif __TREE_WITH_CACHE__

        if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(ptrRootNode->getInnerData()))
        {
            std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrRootNode->getInnerData());

            ptrIndexNode->template print<CacheType, ObjectTypePtr>(os, m_ptrCache, 0, prefix);
        }
        else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(ptrRootNode->getInnerData()))
        {
            std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(ptrRootNode->getInnerData());

            ptrDataNode->print(os, 0, prefix);
        }
    }

    void getCacheState(size_t& nObjectsLinkedList, size_t& nObjectsInMap)
    {
        return m_ptrCache->getCacheState(nObjectsLinkedList, nObjectsInMap);
    }

#ifdef __TREE_WITH_CACHE__
public:
    ErrorCode flush()
    {
        m_ptrCache->flush();

        return ErrorCode::Success;
    }

#ifdef __TRACK_CACHE_FOOTPRINT__
    void applyExistingUpdates(std::shared_ptr<ObjectType> ptrObject
        , std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUIDUpdates, int32_t& nMemoryFootprint)
#else __TRACK_CACHE_FOOTPRINT__
    void applyExistingUpdates(std::shared_ptr<ObjectType> ptrObject
        , std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUIDUpdates)
#endif __TRACK_CACHE_FOOTPRINT__
    {
        if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(ptrObject->getInnerData()))
        {
            std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(ptrObject->getInnerData());

#ifdef __TRACK_CACHE_FOOTPRINT__
            bool bDirty = ptrIndexNode->updateChildrenUIDs(mpUIDUpdates, nMemoryFootprint);
#else __TRACK_CACHE_FOOTPRINT__
            bool bDirty = ptrIndexNode->updateChildrenUIDs(mpUIDUpdates);
#endif __TRACK_CACHE_FOOTPRINT__
            if (bDirty)
            {
                ptrObject->setDirtyFlag(true);
            }

            //auto it = ptrIndexNode->getChildrenBeginIterator();
            //while (it != ptrIndexNode->getChildrenEndIterator())
            //{
            //    if (mpUIDUpdates.find(*it) != mpUIDUpdates.end())
            //    {
            //        ObjectUIDType uidTemp = *it;

            //        *it = *(mpUIDUpdates[*it].first);

            //        mpUIDUpdates.erase(uidTemp);

            //        ptrObject->setDirtyFlag( true);
            //    }
            //    it++;
            //}
        }
        else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>(ptrObject->getInnerData()))
        {
            // Nothing to update in this case!
        }
    }

#ifdef __TRACK_CACHE_FOOTPRINT__
    void applyExistingUpdates(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtNodes
        , std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUIDUpdates, int32_t& nMemoryFootprint)
#else __TRACK_CACHE_FOOTPRINT__
    void applyExistingUpdates(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtNodes
        , std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>& mpUIDUpdates)
#endif __TRACK_CACHE_FOOTPRINT__
    {
        auto it = vtNodes.begin();
        while (it != vtNodes.end())
        {
            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>((*it).second.second->getInnerData()))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>((*it).second.second->getInnerData());

#ifdef __TRACK_CACHE_FOOTPRINT__
                bool bDirty = ptrIndexNode->updateChildrenUIDs(mpUIDUpdates, nMemoryFootprint);
#else __TRACK_CACHE_FOOTPRINT__
                bool bDirty = ptrIndexNode->updateChildrenUIDs(mpUIDUpdates);
#endif __TRACK_CACHE_FOOTPRINT__
                if (bDirty)
                {
                    (*it).second.second->setDirtyFlag(true);
                }
                //auto it_children = ptrIndexNode->getChildrenBeginIterator();
                //while (it_children != ptrIndexNode->getChildrenEndIterator())
                //{
                //    if (mpUIDUpdates.find(*it_children) != mpUIDUpdates.end())
                //    {
                //        ObjectUIDType uidTemp = *it_children;

                //        *it_children = *(mpUIDUpdates[*it_children].first);

                //        mpUIDUpdates.erase(uidTemp);

                //        (*it).second.second->setDirtyFlag( true);
                //    }
                //    it_children++;
                //}
            }
            else //if (std::holds_alternative<std::shared_ptr<DataNodeType>>((*it).second.second->getInnerData()))
            {
                // Nothing to update in this case!
            }
            it++;
        }
    }

#ifdef __TRACK_CACHE_FOOTPRINT__
    void prepareFlush(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtNodes
        , size_t nOffset, size_t& nNewOffset, uint16_t nBlockSize, ObjectUIDType::StorageMedia nMediaType, int32_t& nMemoryFootprint)
#else __TRACK_CACHE_FOOTPRINT__
        void prepareFlush(std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>>>& vtNodes
            , size_t nOffset, size_t& nNewOffset, uint16_t nBlockSize, ObjectUIDType::StorageMedia nMediaType)
#endif __TRACK_CACHE_FOOTPRINT__
    {
        nNewOffset = nOffset;

        std::unordered_map<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, std::shared_ptr<ObjectType>>> mpUIDUpdates;

        for (int idx = 0; idx < vtNodes.size(); idx++)
        {
            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(vtNodes[idx].second.second->getInnerData()))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(vtNodes[idx].second.second->getInnerData());

#ifdef __TRACK_CACHE_FOOTPRINT__
                bool bDirty = ptrIndexNode->updateChildrenUIDs(mpUIDUpdates, nMemoryFootprint);
#else __TRACK_CACHE_FOOTPRINT__
                bool bDirty = ptrIndexNode->updateChildrenUIDs(mpUIDUpdates);
#endif __TRACK_CACHE_FOOTPRINT__
                if (bDirty)
                {
                    vtNodes[idx].second.second->setDirtyFlag(true);
                }

                //auto it = ptrIndexNode->getChildrenBeginIterator();
                //while (it != ptrIndexNode->getChildrenEndIterator())
                //{
                //    if (mpUIDUpdates.find(*it) != mpUIDUpdates.end())
                //    {
                //        ObjectUIDType uidTemp = *it;

                //        *it = *(mpUIDUpdates[*it].first);

                //        mpUIDUpdates.erase(uidTemp);

                //        vtNodes[idx].second.second->setDirtyFlag(true);
                //    }
                //    it++;
                //}

                if (!vtNodes[idx].second.second->getDirtyFlag())
                {
                    vtNodes.erase(vtNodes.begin() + idx); idx--;
                    continue;
                }

                size_t nNodeSize = ptrIndexNode->getSize();

                ObjectUIDType uidUpdated;
                ObjectUIDType::createAddressFromArgs(uidUpdated, nMediaType, IndexNodeType::UID, nNewOffset * nBlockSize, nNodeSize);

                vtNodes[idx].second.first = uidUpdated;

                nNewOffset += std::ceil(nNodeSize / (float)nBlockSize);

                if (mpUIDUpdates.find(vtNodes[idx].first) != mpUIDUpdates.end())
                {
                    throw new std::logic_error("should not occur!");
                }
                else
                {
                    mpUIDUpdates[vtNodes[idx].first] = std::make_pair(uidUpdated, vtNodes[idx].second.second);
                }
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(vtNodes[idx].second.second->getInnerData()))
            {
                if (!vtNodes[idx].second.second->getDirtyFlag())
                {
                    vtNodes.erase(vtNodes.begin() + idx); idx--;
                    continue;
                }

                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(vtNodes[idx].second.second->getInnerData());

                size_t nNodeSize = ptrDataNode->getSize();

                ObjectUIDType uidUpdated;
                ObjectUIDType::createAddressFromArgs(uidUpdated, nMediaType, DataNodeType::UID, nNewOffset * nBlockSize, nNodeSize);

                vtNodes[idx].second.first = uidUpdated;

                nNewOffset += std::ceil(nNodeSize / (float)nBlockSize);

                if (mpUIDUpdates.find(vtNodes[idx].first) != mpUIDUpdates.end())
                {
                    throw new std::logic_error("should not occur!");
                }
                else
                {
                    mpUIDUpdates[vtNodes[idx].first] = std::make_pair(uidUpdated, vtNodes[idx].second.second);
                }
            }
        }
    }
#endif __TREE_WITH_CACHE__
};
