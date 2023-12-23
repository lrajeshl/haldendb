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
#define __POSITION_AWARE_ITEMS__

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
        , m_cktRootNodeKey(std::nullopt)
    {    
        m_ptrCache = std::make_shared<CacheType>(args...);
    }

    template <typename DefaultNodeType>
    void init()
    {
        m_ptrCache->init(this);

#ifdef __POSITION_AWARE_ITEMS__
        m_ptrCache->template createObjectOfType<DefaultNodeType>(m_cktRootNodeKey, std::nullopt);
#else
        m_ptrCache->template createObjectOfType<DefaultNodeType>(m_cktRootNodeKey);
#endif __POSITION_AWARE_ITEMS__
    }

    template <typename IndexNodeType, typename DataNodeType>
    ErrorCode insert(const KeyType& key, const ValueType& value)
    {
#ifdef __CONCURRENT__
        std::vector<std::unique_lock<std::shared_mutex>> vtLocks;
#endif // __CONCURRENT__

#ifdef __POSITION_AWARE_ITEMS__
        std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>>> vtNodes;
        std::optional<ObjectUIDType> uidCurrentNodeParent, uidLastNodeParent;
#else
        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtNodes;
#endif __POSITION_AWARE_ITEMS__

        ObjectTypePtr ptrLastNode = nullptr, ptrCurrentNode = nullptr;
        ObjectUIDType ckLastNode, ckCurrentNode;

#ifdef __CONCURRENT__
        vtLocks.push_back(std::unique_lock<std::shared_mutex>(mutex));
#endif // __CONCURRENT__

        ckCurrentNode = m_cktRootNodeKey.value();

        do
        {
#ifdef __POSITION_AWARE_ITEMS__
            uidCurrentNodeParent = ckLastNode;
            m_ptrCache->getObject(ckCurrentNode, ptrCurrentNode, uidCurrentNodeParent);    //TODO: lock
            if (ptrLastNode!=nullptr &&!uidCurrentNodeParent) 
            {
                throw new std::exception("should not occur!");   // TODO: critical log.
            }
            if (ptrLastNode != nullptr &&uidCurrentNodeParent != ckLastNode)
            {
                throw new std::exception("should not occur!");   // TODO: critical log.
        }

#else
            m_ptrCache->getObject(ckCurrentNode, ptrCurrentNode);    //TODO: lock
#endif __POSITION_AWARE_ITEMS__

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
#ifdef __POSITION_AWARE_ITEMS__
                    vtNodes.push_back(std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>>(ckLastNode, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>(uidLastNodeParent, ptrLastNode)));
#else
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(ckLastNode, ptrLastNode));
#endif __POSITION_AWARE_ITEMS__
                }
                else
                {
#ifdef __CONCURRENT__
                    vtLocks.erase(vtLocks.begin(), vtLocks.begin() + vtLocks.size() - 1);
#endif __CONCURRENT__
                    vtNodes.clear(); //TODO: release locks
                }

                ckLastNode = ckCurrentNode;
                ptrLastNode = ptrCurrentNode;

                uidLastNodeParent = uidCurrentNodeParent;

                ckCurrentNode = ptrIndexNode->getChild(key); // fid it.. there are two kinds of methods..
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data);

                ptrDataNode->insert(key, value);

                if (ptrDataNode->requireSplit(m_nDegree))
                {
#ifdef __POSITION_AWARE_ITEMS__
                    vtNodes.push_back(std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>>(ckLastNode, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>(uidLastNodeParent, ptrLastNode)));
                    vtNodes.push_back(std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>>(ckCurrentNode, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>(uidCurrentNodeParent, ptrCurrentNode)));
#else
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(ckLastNode, ptrLastNode));
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(ckCurrentNode, ptrCurrentNode));
#endif __POSITION_AWARE_ITEMS__
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
        ObjectUIDType ckLHSNode;
        std::optional<ObjectUIDType> ckRHSNode;

        while (vtNodes.size() > 0) 
        {
#ifdef __POSITION_AWARE_ITEMS__
            std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>> prNodeDetails = vtNodes.back();
#else
            std::pair<ObjectUIDType, ObjectTypePtr> prNodeDetails = vtNodes.back();
#endif __POSITION_AWARE_ITEMS__

#ifdef __POSITION_AWARE_ITEMS__
            if (prNodeDetails.second.second == nullptr)
#else
            if (prNodeDetails.second == nullptr)
#endif __POSITION_AWARE_ITEMS__
            {
                if (vtNodes.size() != 1)
                {
                    throw new std::exception("should not occur!");
                }

#ifdef __POSITION_AWARE_ITEMS__
                m_ptrCache->template createObjectOfType<IndexNodeType>(m_cktRootNodeKey, std::nullopt, pivotKey, ckLHSNode, *ckRHSNode);
                m_ptrCache->updateParentUID(ckLHSNode, *m_cktRootNodeKey);
                m_ptrCache->updateParentUID(*ckRHSNode, *m_cktRootNodeKey);
#else
                m_ptrCache->template createObjectOfType<IndexNodeType>(m_cktRootNodeKey, pivotKey, ckLHSNode, * ckRHSNode); 
#endif __POSITION_AWARE_ITEMS__
                break;
            }

#ifdef __POSITION_AWARE_ITEMS__
            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second.second->data);
#else
            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data);
#endif __POSITION_AWARE_ITEMS__

                ptrIndexNode->insert(pivotKey, *ckRHSNode);

                ckRHSNode = std::nullopt;

                if (ptrIndexNode->requireSplit(m_nDegree))
                {
#ifdef __POSITION_AWARE_ITEMS__
                    ErrorCode errCode = ptrIndexNode->template split<std::shared_ptr<CacheType>>(m_ptrCache, ckRHSNode, prNodeDetails.second.first, pivotKey);
#else
                    ErrorCode errCode = ptrIndexNode->template split<std::shared_ptr<CacheType>>(m_ptrCache, ckRHSNode, pivotKey); 
#endif __POSITION_AWARE_ITEMS__

                    if (errCode != ErrorCode::Success)
                    {
                        throw new std::exception("should not occur!");
                    }
                }
            }
#ifdef __POSITION_AWARE_ITEMS__
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*prNodeDetails.second.second->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*prNodeDetails.second.second->data);

                ErrorCode errCode = ptrDataNode->template split<std::shared_ptr<CacheType>, ObjectUIDType>(m_ptrCache, ckRHSNode, prNodeDetails.second.first, pivotKey);
#else
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*prNodeDetails.second->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*prNodeDetails.second->data);

                ErrorCode errCode = ptrDataNode->template split<std::shared_ptr<CacheType>, ObjectUIDType>(m_ptrCache, ckRHSNode, pivotKey);
#endif __POSITION_AWARE_ITEMS__

                if (errCode != ErrorCode::Success)
                {
                    throw new std::exception("should not occur!");
                }                
            }

            ckLHSNode = prNodeDetails.first;

            vtNodes.pop_back();
        }

        return ErrorCode::Success;
        void* current = m_ptrCache->gethead();
        std::cout << "*** ";
        while (current != NULL)
        {
            ObjectUIDType _t = m_ptrCache->getuid_(current);

            std::optional< ObjectUIDType> pk;
            ObjectTypePtr ptrObject = nullptr;
            m_ptrCache->getObject_(_t, ptrObject, pk);

            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrObject->data))
            {
                std::shared_ptr<IndexNodeType> ptrIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrObject->data);

                ptrIndexNode->printkeys();
                //ckCurrentNode = ptrIndexNode->getChild(key);
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrObject->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrObject->data);

                ptrDataNode->printkeys();
                //errCode = ptrDataNode->getValue(key, value);

                //break;
            }
            current = m_ptrCache->getnext(current);
            if (current != NULL)
                std::cout << " -> ";
        }
        std::cout << " ...." << std::endl;
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

        ObjectUIDType ckCurrentNode = m_cktRootNodeKey.value();
        do
        {
            std::optional<ObjectUIDType> keyParent;
            ObjectTypePtr prNodeDetails = nullptr;
            m_ptrCache->getObject(ckCurrentNode, prNodeDetails, keyParent);    //TODO: lock

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

                ckCurrentNode = ptrIndexNode->getChild(key);
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

#ifdef __POSITION_AWARE_ITEMS__
        std::vector<std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>>> vtNodes;
        std::optional<ObjectUIDType> uidCurrentNodeParent, uidLastNodeParent;
#else
        std::vector<std::pair<ObjectUIDType, ObjectTypePtr>> vtNodes;
#endif __POSITION_AWARE_ITEMS__

        ObjectTypePtr ptrLastNode = nullptr, ptrCurrentNode = nullptr;
        ObjectUIDType ckLastNode, ckCurrentNode;

#ifdef __CONCURRENT__
        vtLocks.push_back(std::unique_lock<std::shared_mutex>(mutex));
#endif __CONCURRENT__

        ckCurrentNode = m_cktRootNodeKey.value();

        do
        {
#ifdef __POSITION_AWARE_ITEMS__
            m_ptrCache->getObject(ckCurrentNode, ptrCurrentNode, uidCurrentNodeParent);    //TODO: lock
#else
            m_ptrCache->getObject(ckCurrentNode, ptrCurrentNode);    //TODO: lock
#endif __POSITION_AWARE_ITEMS__
            
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
#ifdef __POSITION_AWARE_ITEMS__
                    vtNodes.push_back(std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>>(ckLastNode, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>(uidLastNodeParent, ptrLastNode)));
#else
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(ckLastNode, ptrLastNode));
#endif __POSITION_AWARE_ITEMS__
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

                ckLastNode = ckCurrentNode;
                ptrLastNode = ptrCurrentNode;

                ckCurrentNode = ptrIndexNode->getChild(key); // fid it.. there are two kinds of methods..
            }
            else if (std::holds_alternative<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data))
            {
                std::shared_ptr<DataNodeType> ptrDataNode = std::get<std::shared_ptr<DataNodeType>>(*ptrCurrentNode->data);

                ptrDataNode->remove(key);

                if (ptrDataNode->requireMerge(m_nDegree))
                {
#ifdef __POSITION_AWARE_ITEMS__
                    vtNodes.push_back(std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>>(ckLastNode, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>(uidLastNodeParent, ptrLastNode)));
                    vtNodes.push_back(std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>>(ckCurrentNode, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>(uidCurrentNodeParent, ptrCurrentNode)));
#else
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(ckLastNode, ptrLastNode));
                    vtNodes.push_back(std::pair<ObjectUIDType, ObjectTypePtr>(ckCurrentNode, ptrCurrentNode));
#endif __POSITION_AWARE_ITEMS__
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
#ifdef __POSITION_AWARE_ITEMS__
            std::pair<ObjectUIDType, std::pair<std::optional<ObjectUIDType>, ObjectTypePtr>> prNodeDetails = vtNodes.back();
#else
            std::pair<ObjectUIDType, ObjectTypePtr> prNodeDetails = vtNodes.back();
#endif __POSITION_AWARE_ITEMS__

#ifdef __POSITION_AWARE_ITEMS__
            if (prNodeDetails.second.second == nullptr)
#else
            if (prNodeDetails.second == nullptr)
#endif __POSITION_AWARE_ITEMS__
            {
                if (vtNodes.size() != 1)
                {
                    throw new std::exception("should not occur!");
                }

                break;
            }

#ifdef __POSITION_AWARE_ITEMS__
            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrParentIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second.second->data);
#else
            if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data))
            {
                std::shared_ptr<IndexNodeType> ptrParentIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*prNodeDetails.second->data);
#endif __POSITION_AWARE_ITEMS__

                if (std::holds_alternative<std::shared_ptr<IndexNodeType>>(*ptrChildNode->data))
                {
                    std::optional<ObjectUIDType> uidToDelete = std::nullopt;

                    std::shared_ptr<IndexNodeType> ptrChildIndexNode = std::get<std::shared_ptr<IndexNodeType>>(*ptrChildNode->data);

                    if (ptrChildIndexNode->requireMerge(m_nDegree))
                    {
#ifdef __POSITION_AWARE_ITEMS__
                        ptrParentIndexNode->template rebalanceIndexNode<std::shared_ptr<CacheType>, shared_ptr<IndexNodeType>>(m_ptrCache, ptrChildIndexNode, key, m_nDegree, ckChildNode, uidToDelete, prNodeDetails.second.first);
#else
                        ptrParentIndexNode->template rebalanceIndexNode<std::shared_ptr<CacheType>, shared_ptr<IndexNodeType>>(m_ptrCache, ptrChildIndexNode, key, m_nDegree, ckChildNode, uidToDelete);
#endif __POSITION_AWARE_ITEMS__


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
#ifdef __POSITION_AWARE_ITEMS__
            ptrChildNode = prNodeDetails.second.second;
#else
            ptrChildNode = prNodeDetails.second; 
#endif __POSITION_AWARE_ITEMS__
            vtNodes.pop_back();
        }
        
        return ErrorCode::Success;
    }

    template <typename IndexNodeType, typename DataNodeType>
    void print()
    {
        /*
        ObjectTypePtr ptrRootNode = nullptr;
        m_ptrCache->getObjectOfType(m_cktRootNodeKey.value(), ptrRootNode);

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
        */
    }

public:
    CacheErrorCode keyUpdate(std::optional<ObjectUIDType>& uidParent, ObjectUIDType uidOld, ObjectUIDType uidNew)
    {
        if (!uidParent)
        {   
            // Update for Root... should not occur
            throw new std::exception("should not occur!");
        }

        std::optional< ObjectUIDType> t;
        ObjectTypePtr ptrObject = nullptr;
        m_ptrCache->getObject_(*uidParent, ptrObject, t);

        if (ptrObject == nullptr)
        {
            throw new std::exception("should not occur!");   // TODO: critical log.
        }

        //std::cout << "Type at the 1st index: " << typeid(INodeType).name() << std::endl;

        if (std::holds_alternative<std::shared_ptr<INodeType>>(*ptrObject->data))
        {
            std::shared_ptr<INodeType> ptrCoreObject = std::get<std::shared_ptr<INodeType>>(*ptrObject->data);

            ptrCoreObject->updateChildUID(uidOld, uidNew);
        }
        else //if (std::holds_alternative<std::shared_ptr<DNodeType>>(*ptrObject->data))
        {
            throw new std::exception("should not occur!");   // TODO: critical log.
            //std::shared_ptr<DNodeType> ptrCoreObject = std::get<std::shared_ptr<DNodeType>>(*ptrObject->data);

            //ptrCoreObject->updateChildUID();
        }

        return CacheErrorCode::Success;
    }

    CacheErrorCode keysUpdate(std::vector<std::pair<ObjectUIDType, std::pair<ObjectUIDType, ObjectUIDType>>> vtUpdatedUIDs)
    {
        return CacheErrorCode::Success;
    }
};