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

//#define __CONCURRENT__
#define __PARENT_AWARE_NODES__

template<typename T>
using unpack = typename T::type;


template <typename ICallback, typename KeyType, typename ValueType, typename CacheType>
class BPlusStore : public ICallback
{
    typedef CacheType::ObjectUIDType ObjectUIDType;
    typedef CacheType::ObjectType ObjectType;
    typedef CacheType::ObjectTypePtr ObjectTypePtr;
    typedef CacheType::ObjectCoreTypes ObjectCoreTypes;

    using DNodeType = typename std::tuple_element<0, typename ObjectType::ObjectCoreTypes>::type;
    using INodeType = typename std::tuple_element<1, typename ObjectType::ObjectCoreTypes>::type;

private:
    uint32_t m_nDegree;
    std::shared_ptr<CacheType> m_ptrCache;
    std::optional<ObjectUIDType> m_cktRootNodeKey;
    mutable std::shared_mutex mutex;

public:
    ~BPlusStore()
    {
    }

    template<typename... CacheArgs>
    BPlusStore(uint32_t nDegree, CacheArgs... args)
        : m_nDegree(nDegree)
    {    
        //std::cout << "Type in MyParameterPack at index 1: " << typeid(INodeType).name() << std::endl;

        m_ptrCache = std::make_shared<CacheType>(args...);
    }

    template <typename DefaultNodeType>
    void init()
    {
        m_ptrCache->init(this);
        m_cktRootNodeKey = m_ptrCache->template createObjectOfType<DefaultNodeType>();
    }

    template <typename IndexNodeType, typename DataNodeType>
    ErrorCode insert(const KeyType& key, const ValueType& value)
    {
#ifdef __CONCURRENT__
        std::vector<std::unique_lock<std::shared_mutex>> vtLocks;
#endif // __CONCURRENT__

        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtNodes;
        ObjectTypePtr ptrLastNode = nullptr, ptrCurrentNode = nullptr;
        ObjectUIDType uidLastNode, uidCurrentNode;

#ifdef __CONCURRENT__
        vtLocks.push_back(std::unique_lock<std::shared_mutex>(mutex));
#endif // __CONCURRENT__

        uidCurrentNode = m_cktRootNodeKey.value();

        do
        {
            ptrCurrentNode = m_ptrCache->getObject(uidCurrentNode);    //TODO: lock

#ifdef __CONCURRENT__
            vtLocks.push_back(std::unique_lock<std::shared_mutex>(ptrCurrentNode->mutex));
#endif __CONCURRENT__

            if (ptrCurrentNode == nullptr)
            {
                throw new std::exception("should not occur!");   // TODO: critical log.
            }

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrCurrentNode->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrCurrentNode->data);

                if (ptrIndexNode->canTriggerSplit(m_nDegree))
                {
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidLastNode, ptrLastNode));
                }
                else
                {
#ifdef __CONCURRENT__
                    vtLocks.erase(vtLocks.begin(), vtLocks.begin() + vtLocks.size() - 1);
#endif __CONCURRENT__
                    vtNodes.clear(); //TODO: release locks
                }

                uidLastNode = uidCurrentNode;
                ptrLastNode = ptrCurrentNode;

                uidCurrentNode = ptrIndexNode->getChild(key); // fid it.. there are two kinds of methods..
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data);

                ptrDataNode->insert(key, value);

                if (ptrDataNode->requireSplit(m_nDegree))
                {
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidLastNode, ptrLastNode));
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidCurrentNode, ptrCurrentNode));
                }
                else
                {
#ifdef __CONCURRENT__
                    vtLocks.clear();
#endif __CONCURRENT__
                    vtNodes.clear(); //TODO: release locks
                }

                break;
            }
        } while (true);

        KeyType pivotKey;
        ObjectUIDType uidLHSNode;
        std::optional<ObjectUIDType> uidRHSNode;

#ifdef __PARENT_AWARE_NODES__
        ObjectTypePtr ptrLHSNode = nullptr, ptrRHSNode = nullptr;
#endif __PARENT_AWARE_NODES__

        while (vtNodes.size() > 0) 
        {
            std::pair<ObjectUIDType, ObjectTypePtr> prNodeDetails = vtNodes.back();

            if (prNodeDetails.second == nullptr)
            {
                if (vtNodes.size() != 1)
                {
                    throw new std::exception("should not occur!");
                }

                m_cktRootNodeKey = m_ptrCache->template createObjectOfType<IndexNodeType>(pivotKey, uidLHSNode, *uidRHSNode, std::nullopt);

#ifdef __PARENT_AWARE_NODES__
                if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrLHSNode->data))
                {
                    std::get<std::shared_ptr<IndexNodeType>>(*ptrLHSNode->data)->m_uidParent = m_cktRootNodeKey;
                    std::get<std::shared_ptr<IndexNodeType>>(*ptrRHSNode->data)->m_uidParent = m_cktRootNodeKey;
                }
                else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrLHSNode->data))
                {
                    std::get<std::shared_ptr<DataNodeType>>(*ptrLHSNode->data)->m_uidParent = m_cktRootNodeKey;
                    std::get<std::shared_ptr<DataNodeType>>(*ptrRHSNode->data)->m_uidParent = m_cktRootNodeKey;
                }
#endif __PARENT_AWARE_NODES__

                break;
            }

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data);

                ptrIndexNode->insert(pivotKey, *uidRHSNode);

                uidRHSNode = std::nullopt;

                if (ptrIndexNode->requireSplit(m_nDegree))
                {
#ifdef __PARENT_AWARE_NODES__
                    ErrorCode errCode = ptrIndexNode->template split<std::shared_ptr<CacheType>, ObjectTypePtr>(m_ptrCache, uidRHSNode, ptrRHSNode, pivotKey);
#else
                    ErrorCode errCode = ptrIndexNode->template split<std::shared_ptr<CacheType>>(m_ptrCache, uidRHSNode, pivotKey);
#endif __PARENT_AWARE_NODES__

                    if (errCode != ErrorCode::Success)
                    {
                        throw new std::exception("should not occur!");
                    }
                }
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*prNodeDetails.second->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*prNodeDetails.second->data);

#ifdef __PARENT_AWARE_NODES__
                ErrorCode errCode = ptrDataNode->template split<std::shared_ptr<CacheType>, ObjectTypePtr>(m_ptrCache, uidRHSNode, ptrRHSNode, pivotKey);
#else
                ErrorCode errCode = ptrDataNode->template split<std::shared_ptr<CacheType>>(m_ptrCache, uidRHSNode, pivotKey);
#endif __PARENT_AWARE_NODES__

                if (errCode != ErrorCode::Success)
                {
                    throw new std::exception("should not occur!");
                }                
            }

            uidLHSNode = prNodeDetails.first;
#ifdef __PARENT_AWARE_NODES__
            ptrLHSNode = prNodeDetails.second;
#endif __PARENT_AWARE_NODES__

            vtNodes.pop_back();
        }

        return ErrorCode::Success;
    }

    template <typename IndexNodeType, typename DataNodeType>
    ErrorCode search(const KeyType& key, ValueType& value)
    {
        ErrorCode errCode = ErrorCode::Error;

#ifdef __CONCURRENT__
        std::vector<std::shared_lock<std::shared_mutex>> vtLocks;
        vtLocks.push_back(std::shared_lock<std::shared_mutex>(mutex));
#endif __CONCURRENT__

        ObjectUIDType uidCurrentNode = m_cktRootNodeKey.value();
        do
        {
            ObjectTypePtr prNodeDetails = m_ptrCache->getObject(uidCurrentNode);    //TODO: lock

#ifdef __CONCURRENT__
            vtLocks.push_back(std::shared_lock<std::shared_mutex>(prNodeDetails->mutex));
            vtLocks.erase(vtLocks.begin());
#endif __CONCURRENT__

            if (prNodeDetails == nullptr)
            {
                throw new std::exception("should not occur!");
            }

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*prNodeDetails->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*prNodeDetails->data);

                uidCurrentNode = ptrIndexNode->getChild(key);
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*prNodeDetails->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*prNodeDetails->data);

                errCode = ptrDataNode->getValue(key, value);

                break;
            }
        } while (true);

        return errCode;
    }

    template <typename IndexNodeType, typename DataNodeType>
    ErrorCode remove(const KeyType& key)
    {
#ifdef __CONCURRENT__
        std::vector<std::unique_lock<std::shared_mutex>> vtLocks;
#endif __CONCURRENT__

        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtNodes;
        ObjectTypePtr ptrLastNode = nullptr, ptrCurrentNode = nullptr;
        ObjectUIDType uidLastNode, uidCurrentNode;

#ifdef __CONCURRENT__
        vtLocks.push_back(std::unique_lock<std::shared_mutex>(mutex));
#endif __CONCURRENT__

        uidCurrentNode = m_cktRootNodeKey.value();

        do
        {
            ptrCurrentNode = m_ptrCache->getObject(uidCurrentNode);    //TODO: lock
            
#ifdef __CONCURRENT__
            vtLocks.push_back(std::unique_lock<std::shared_mutex>(ptrCurrentNode->mutex));
#endif __CONCURRENT__

            if (ptrCurrentNode == nullptr)
            {
                throw new std::exception("should not occur!");
            }

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrCurrentNode->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrCurrentNode->data);

                if (ptrIndexNode->canTriggerMerge(m_nDegree))
                {
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidLastNode, ptrLastNode));
                }
                else
                {
#ifdef __CONCURRENT__
                    if (vtLocks.size() > 3) //TODO: 3 seems to be working.. but how and why.. investiage....!
                    {
                        vtLocks.erase(vtLocks.begin());
                    }
#endif __CONCURRENT__
                    vtNodes.clear(); //TODO: release locks
                }

                uidLastNode = uidCurrentNode;
                ptrLastNode = ptrCurrentNode;

                uidCurrentNode = ptrIndexNode->getChild(key); // fid it.. there are two kinds of methods..
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data);

                ptrDataNode->remove(key);

                if (ptrDataNode->requireMerge(m_nDegree))
                {
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidLastNode, ptrLastNode));
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(uidCurrentNode, ptrCurrentNode));
                }
                else
                {
#ifdef __CONCURRENT__
                    vtLocks.clear();
#endif __CONCURRENT__
                    vtNodes.clear(); //TODO: release locks
                }

                break;
            }
        } while (true);

        ObjectUIDType ckChildNode;
        ObjectTypePtr ptrChildNode = nullptr;

        while (vtNodes.size() > 0)
        {
            std::pair<ObjectUIDType, ObjectTypePtr> prNodeDetails = vtNodes.back();

            if (prNodeDetails.second == nullptr)
            {
                if (vtNodes.size() != 1)
                {
                    throw new std::exception("should not occur!");
                }

                break;
            }

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrParentIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data);

                if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrChildNode->data))
                {
                    std::optional<ObjectUIDType> uidToDelete = std::nullopt;

                    std::shared_ptr<IndexNodeType> ptrChildIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrChildNode->data);

                    if (ptrChildIndexNode->requireMerge(m_nDegree))
                    {
                        ptrParentIndexNode->template rebalanceIndexNode<std::shared_ptr<CacheType>, shared_ptr<IndexNodeType>>(m_ptrCache, ptrChildIndexNode, key, m_nDegree, ckChildNode, uidToDelete);

                        if (uidToDelete)
                        {
#ifdef __CONCURRENT__
                            if (*uidToDelete == ckChildNode)
                            {
                                auto it = vtLocks.begin();
                                while (it != vtLocks.end()) {
                                    if ((*it).mutex() == &ptrChildNode->mutex)
                                    {
                                        break;
                                    }
                                    it++;
                                }

                                if (it != vtLocks.end())
                                    vtLocks.erase(it);
                            }
#endif __CONCURRENT__

                            m_ptrCache->remove(*uidToDelete);
                        }

                        if (ptrParentIndexNode->getKeysCount() == 0)
                        {
                            m_cktRootNodeKey = ptrParentIndexNode->getChildAt(0);
                            //throw new exception("should not occur!");
                        }
                    }
                }
                else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrChildNode->data))
                {
                    std::optional<ObjectUIDType> uidToDelete = std::nullopt;

                    std::shared_ptr<DataNodeType> ptrChildDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrChildNode->data);
                    ptrParentIndexNode->template rebalanceDataNode<std::shared_ptr<CacheType>, shared_ptr<DataNodeType>>(m_ptrCache, ptrChildDataNode, key, m_nDegree, ckChildNode, uidToDelete);

                    if (uidToDelete)
                    {
#ifdef __CONCURRENT__
                        if (*uidToDelete == ckChildNode)
                        {
                            auto it = vtLocks.begin();
                            while (it != vtLocks.end()) {
                                if ((*it).mutex() == &ptrChildNode->mutex)
                                {
                                    break;
                                }
                                it++;
                            }

                            if (it != vtLocks.end())
                                vtLocks.erase(it);
                        }
#endif __CONCURRENT__

                        m_ptrCache->remove(*uidToDelete);
                    }

                    if (ptrParentIndexNode->getKeysCount() == 0)
                    {
                        m_cktRootNodeKey = ptrParentIndexNode->getChildAt(0);
                        //throw new exception("should not occur!");
                    }
                }
            }

            ckChildNode = prNodeDetails.first;
            ptrChildNode = prNodeDetails.second;

            vtNodes.pop_back();
        }

        return ErrorCode::Success;
    }

    template <typename IndexNodeType, typename DataNodeType>
    void print()
    {
        ObjectTypePtr ptrRootNode = m_ptrCache->getObjectOfType(m_cktRootNodeKey.value());
        if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrRootNode->data))
        {
            std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrRootNode->data);

            ptrIndexNode->template print<std::shared_ptr<CacheType>, ObjectTypePtr, DataNodeType>(m_ptrCache, 0);
        }
        else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrRootNode->data))
        {
            std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrRootNode->data);

            ptrDataNode->print(0);
        }
    }

public:
    CacheErrorCode keyUpdate(ObjectTypePtr ptrChildNode, ObjectUIDType uidChildOld, ObjectUIDType uidChildNew)
    {
#ifdef __CONCURRENT__
        std::unique_lock<std::shared_mutex> lock_node(ptrChildNode->mutex);
#endif __CONCURRENT__

        if (ptrChildNode == nullptr)
        {
            throw new std::exception("should not occur!");   // TODO: critical log.
        }

        ObjectUIDType uidParent;

        std::cout << "Type at the 1st index: " << typeid(INodeType).name() << std::endl;

        if (std::holds_alternative<std::shared_ptr<INodeType>>(*ptrChildNode->data))
        {
            std::shared_ptr<INodeType> ptrCoreObject = std::get<std::shared_ptr<INodeType>>(*ptrChildNode->data);

            uidParent = *ptrCoreObject->m_uidParent;
        }
        else if (std::holds_alternative<std::shared_ptr<DNodeType>>(*ptrChildNode->data))
        {
            std::shared_ptr<DNodeType> ptrCoreObject = std::get<std::shared_ptr<DNodeType>>(*ptrChildNode->data);

            uidParent = *ptrCoreObject->m_uidParent;
        }

        ObjectTypePtr ptrParentNode = m_ptrCache->getObject(uidParent);

#ifdef __CONCURRENT__
        std::unique_lock<std::shared_mutex> lock_parent(ptrParentNode->mutex);
        lock_node.unlock();
#endif __CONCURRENT__

        if (std::holds_alternative<std::shared_ptr<INodeType>>(*ptrParentNode->data))
        {
            std::shared_ptr<INodeType> ptrCoreObject = std::get<std::shared_ptr<INodeType>>(*ptrParentNode->data);

            if (ptrCoreObject->updateChildUID(uidChildOld, uidChildNew) == ErrorCode::Success)
                return CacheErrorCode::Success;
        }
        else //if (std::holds_alternative<std::shared_ptr<DNodeType>>(*ptrParentNode->data))
        {
            throw new std::exception("should not occur!");   // TODO: critical log.
        }

        return CacheErrorCode::Success;
    }

    CacheErrorCode keysUpdate(std::unordered_map<ObjectUIDType, ObjectUIDType> mpUIDsUpdate)
    {
#ifdef __CONCURRENT__
        std::unique_lock<std::shared_mutex> lock_updated_uids(this->m_mtxUIDsUpdate);
#endif __CONCURRENT__

        this->m_mpUIDsUpdate = std::move(mpUIDsUpdate);

#ifdef __CONCURRENT__
        lock_updated_uids.unlock();
#endif __CONCURRENT__

        // m_ptrCallback->keysUpdate(mpUIDsUpdate); TODO: When cache is shared across multiple trees.

        return CacheErrorCode::Success;
    }
};