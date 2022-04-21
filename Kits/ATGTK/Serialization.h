//--------------------------------------------------------------------------------------
// Serialization.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stack>
#include <type_traits>
#include <utility>
#include <vector>

#define ENABLE_IF_INTEGRAL(_T_) typename std::enable_if<std::is_integral<_T_>::value>::type* = nullptr
#define ENABLE_IF_NOT_INTEGRAL(_T_) typename std::enable_if<!std::is_integral<_T_>::value>::type* = nullptr


//--------------------------------------------------------------------------------------
// Interfaces for a generic Visitor implementation
//--------------------------------------------------------------------------------------
#pragma region Visitor
namespace ATG
{
    class IConstVisitor
    {
    public:
        virtual ~IConstVisitor() {}

        virtual void VisitPrimitiveElement(int8_t elt) = 0;
        virtual void VisitPrimitiveElement(uint8_t elt) = 0;
        virtual void VisitPrimitiveElement(int16_t elt) = 0;
        virtual void VisitPrimitiveElement(uint16_t elt) = 0;
        virtual void VisitPrimitiveElement(int32_t elt) = 0;
        virtual void VisitPrimitiveElement(uint32_t elt) = 0;
        virtual void VisitPrimitiveElement(int64_t elt) = 0;
        virtual void VisitPrimitiveElement(uint64_t elt) = 0;
        virtual void VisitPrimitiveElement(wchar_t elt) = 0;
        virtual void VisitPrimitiveElement(char elt) = 0;


        virtual void VisitPrimitiveCollection(const   int8_t *elts, size_t count) = 0;
        virtual void VisitPrimitiveCollection(const  uint8_t *elts, size_t count) = 0;
        virtual void VisitPrimitiveCollection(const  int16_t *elts, size_t count) = 0;
        virtual void VisitPrimitiveCollection(const uint16_t *elts, size_t count) = 0;
        virtual void VisitPrimitiveCollection(const  int32_t *elts, size_t count) = 0;
        virtual void VisitPrimitiveCollection(const uint32_t *elts, size_t count) = 0;
        virtual void VisitPrimitiveCollection(const  int64_t *elts, size_t count) = 0;
        virtual void VisitPrimitiveCollection(const uint64_t *elts, size_t count) = 0;
        virtual void VisitPrimitiveCollection(const  wchar_t *elts, size_t count) = 0;
        virtual void VisitPrimitiveCollection(const     char *elts, size_t count) = 0;

        virtual void VisitElement(class ConstVisitorContext &) = 0;
        virtual void VisitCollection(size_t count, class ConstVisitorContext &) = 0;
    };

    template<typename ConstVisitorTemplates>
    class ImplementIConstVisitor : public IConstVisitor
    {
    public:

        void VisitPrimitiveElement(int8_t elt) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(uint8_t elt) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(int16_t elt) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(uint16_t elt) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(int32_t elt) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(uint32_t elt) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(int64_t elt) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(uint64_t elt) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(wchar_t elt) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(char elt) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }

        void VisitPrimitiveCollection(const   int8_t *elts, size_t count) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveCollection(elts, count); }
        void VisitPrimitiveCollection(const  uint8_t *elts, size_t count) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveCollection(elts, count); }
        void VisitPrimitiveCollection(const  int16_t *elts, size_t count) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveCollection(elts, count); }
        void VisitPrimitiveCollection(const uint16_t *elts, size_t count) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveCollection(elts, count); }
        void VisitPrimitiveCollection(const  int32_t *elts, size_t count) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveCollection(elts, count); }
        void VisitPrimitiveCollection(const uint32_t *elts, size_t count) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveCollection(elts, count); }
        void VisitPrimitiveCollection(const  int64_t *elts, size_t count) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveCollection(elts, count); }
        void VisitPrimitiveCollection(const uint64_t *elts, size_t count) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveCollection(elts, count); }
        void VisitPrimitiveCollection(const  wchar_t *elts, size_t count) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveCollection(elts, count); }
        void VisitPrimitiveCollection(const     char *elts, size_t count) override { static_cast<ConstVisitorTemplates*>(this)->ImplementVisitPrimitiveCollection(elts, count); }

        void VisitElement(class ConstVisitorContext &ctx) override
        {
            static_cast<ConstVisitorTemplates*>(this)->ImplementVisitElement(ctx);
        }

        void VisitCollection(size_t count, class ConstVisitorContext &ctx) override
        {
            static_cast<ConstVisitorTemplates*>(this)->ImplementVisitCollection(count, ctx);
        }

    };

#define DECLARE_GET_BUFFER_INTERFACE(ELT_TYPE) \
    class IGetBuffer_ ## ELT_TYPE \
    { \
    public: \
        virtual ~IGetBuffer_ ## ELT_TYPE() {} \
        virtual ELT_TYPE *GetBuffer(size_t count) = 0; \
    };

    DECLARE_GET_BUFFER_INTERFACE(int8_t)
    DECLARE_GET_BUFFER_INTERFACE(uint8_t)
    DECLARE_GET_BUFFER_INTERFACE(int16_t)
    DECLARE_GET_BUFFER_INTERFACE(uint16_t)
    DECLARE_GET_BUFFER_INTERFACE(int32_t)
    DECLARE_GET_BUFFER_INTERFACE(uint32_t)
    DECLARE_GET_BUFFER_INTERFACE(int64_t)
    DECLARE_GET_BUFFER_INTERFACE(uint64_t)
    DECLARE_GET_BUFFER_INTERFACE(wchar_t)
    DECLARE_GET_BUFFER_INTERFACE(char)
#undef DECLARE_GET_BUFFER_INTERFACE

    template<typename EltType>
    struct SelectIGetBufferBase
    {
    };

#define SPECIALIZE_SELECT_GET_BUFFER(ELT_TYPE) \
    template<> \
    struct SelectIGetBufferBase<ELT_TYPE> \
    { \
        using BaseType = IGetBuffer_ ## ELT_TYPE; \
    };

    SPECIALIZE_SELECT_GET_BUFFER(int8_t)
    SPECIALIZE_SELECT_GET_BUFFER(uint8_t)
    SPECIALIZE_SELECT_GET_BUFFER(int16_t)
    SPECIALIZE_SELECT_GET_BUFFER(uint16_t)
    SPECIALIZE_SELECT_GET_BUFFER(int32_t)
    SPECIALIZE_SELECT_GET_BUFFER(uint32_t)
    SPECIALIZE_SELECT_GET_BUFFER(int64_t)
    SPECIALIZE_SELECT_GET_BUFFER(uint64_t)
    SPECIALIZE_SELECT_GET_BUFFER(wchar_t)
    SPECIALIZE_SELECT_GET_BUFFER(char)
#undef SPECIALIZE_SELECT_GET_BUFFER

    template<typename EltType>
    using IGetBuffer_t = typename SelectIGetBufferBase<EltType>::BaseType;

    class IVisitor
    {
    public:
        virtual ~IVisitor() {}

        virtual void VisitPrimitiveElement(int8_t &elt) = 0;
        virtual void VisitPrimitiveElement(uint8_t &elt) = 0;
        virtual void VisitPrimitiveElement(int16_t &elt) = 0;
        virtual void VisitPrimitiveElement(uint16_t &elt) = 0;
        virtual void VisitPrimitiveElement(int32_t &elt) = 0;
        virtual void VisitPrimitiveElement(uint32_t &elt) = 0;
        virtual void VisitPrimitiveElement(int64_t &elt) = 0;
        virtual void VisitPrimitiveElement(uint64_t &elt) = 0;
        virtual void VisitPrimitiveElement(wchar_t &elt) = 0;
        virtual void VisitPrimitiveElement(char &elt) = 0;

        virtual void VisitPrimitiveCollection(IGetBuffer_t<   int8_t> &getBuffer) = 0;
        virtual void VisitPrimitiveCollection(IGetBuffer_t<  uint8_t> &getBuffer) = 0;
        virtual void VisitPrimitiveCollection(IGetBuffer_t<  int16_t> &getBuffer) = 0;
        virtual void VisitPrimitiveCollection(IGetBuffer_t< uint16_t> &getBuffer) = 0;
        virtual void VisitPrimitiveCollection(IGetBuffer_t<  int32_t> &getBuffer) = 0;
        virtual void VisitPrimitiveCollection(IGetBuffer_t< uint32_t> &getBuffer) = 0;
        virtual void VisitPrimitiveCollection(IGetBuffer_t<  int64_t> &getBuffer) = 0;
        virtual void VisitPrimitiveCollection(IGetBuffer_t< uint64_t> &getBuffer) = 0;
        virtual void VisitPrimitiveCollection(IGetBuffer_t<  wchar_t> &getBuffer) = 0;
        virtual void VisitPrimitiveCollection(IGetBuffer_t<     char> &getBuffer) = 0;

        virtual void VisitElement(class VisitorContext &) = 0;
        virtual void VisitCollection(size_t &count, class VisitorContext &ctx) = 0;
    };

    template<typename VisitorTemplates>
    class ImplementIVisitor : public IVisitor
    {
        void VisitPrimitiveElement(int8_t &elt) override { static_cast<VisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(uint8_t &elt) override { static_cast<VisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(int16_t &elt) override { static_cast<VisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(uint16_t &elt) override { static_cast<VisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(int32_t &elt) override { static_cast<VisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(uint32_t &elt) override { static_cast<VisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(int64_t &elt) override { static_cast<VisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(uint64_t &elt) override { static_cast<VisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(wchar_t &elt) override { static_cast<VisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }
        void VisitPrimitiveElement(char &elt) override { static_cast<VisitorTemplates*>(this)->ImplementVisitPrimitiveElement(elt); }

        void VisitPrimitiveCollection(IGetBuffer_t<   int8_t> &getBuffer) override { static_cast<VisitorTemplates*>(this)->template ImplementVisitPrimitiveCollection<   int8_t>(getBuffer); }
        void VisitPrimitiveCollection(IGetBuffer_t<  uint8_t> &getBuffer) override { static_cast<VisitorTemplates*>(this)->template ImplementVisitPrimitiveCollection<  uint8_t>(getBuffer); }
        void VisitPrimitiveCollection(IGetBuffer_t<  int16_t> &getBuffer) override { static_cast<VisitorTemplates*>(this)->template ImplementVisitPrimitiveCollection<  int16_t>(getBuffer); }
        void VisitPrimitiveCollection(IGetBuffer_t< uint16_t> &getBuffer) override { static_cast<VisitorTemplates*>(this)->template ImplementVisitPrimitiveCollection< uint16_t>(getBuffer); }
        void VisitPrimitiveCollection(IGetBuffer_t<  int32_t> &getBuffer) override { static_cast<VisitorTemplates*>(this)->template ImplementVisitPrimitiveCollection<  int32_t>(getBuffer); }
        void VisitPrimitiveCollection(IGetBuffer_t< uint32_t> &getBuffer) override { static_cast<VisitorTemplates*>(this)->template ImplementVisitPrimitiveCollection< uint32_t>(getBuffer); }
        void VisitPrimitiveCollection(IGetBuffer_t<  int64_t> &getBuffer) override { static_cast<VisitorTemplates*>(this)->template ImplementVisitPrimitiveCollection<  int64_t>(getBuffer); }
        void VisitPrimitiveCollection(IGetBuffer_t< uint64_t> &getBuffer) override { static_cast<VisitorTemplates*>(this)->template ImplementVisitPrimitiveCollection< uint64_t>(getBuffer); }
        void VisitPrimitiveCollection(IGetBuffer_t<  wchar_t> &getBuffer) override { static_cast<VisitorTemplates*>(this)->template ImplementVisitPrimitiveCollection<  wchar_t>(getBuffer); }
        void VisitPrimitiveCollection(IGetBuffer_t<     char> &getBuffer) override { static_cast<VisitorTemplates*>(this)->template ImplementVisitPrimitiveCollection<     char>(getBuffer); }

        void VisitElement(class VisitorContext &ctx) override
        {
            static_cast<VisitorTemplates*>(this)->ImplementVisitElement(ctx);
        }

        void VisitCollection(size_t &count, class VisitorContext &ctx) override
        {
            static_cast<VisitorTemplates*>(this)->ImplementVisitCollection(count, ctx);
        }
    };

} // namespace ATG
#pragma endregion

//--------------------------------------------------------------------------------------
// Visitor Context is a stack machine used to implement a non-recursive class visitor
//--------------------------------------------------------------------------------------
#pragma region Visitor Context
namespace ATG
{
    template<typename T> class ClassVisitorActions;

    // Prevents redundant creation of ClassVisitors by caching a static pointer
    // each time a new ClassVisitorTemplate is created. Rather then using an indefinite
    // singleton instance for each one, this class also manages the lifetimes
    // and will delete the class visitors when it is destructed.
    class ClassVisitorCache
    {
        // Only need a virtual destructor
        class IHolder { public: virtual ~IHolder() {} };

        // This class keeps track of the address of a static pointer
        // The purpose of this class is to delete the object and then
        // clear the pointer
        template<typename T>
        class VisitorActionsHolder : public IHolder
        {
        public:
            VisitorActionsHolder(T **visitorActions)
                : m_visitorActions(visitorActions)
            {
            }

            ~VisitorActionsHolder()
            {
                if (m_visitorActions)
                {
                    T *actions = *m_visitorActions;
                    *m_visitorActions = nullptr;
                    m_visitorActions = nullptr;
                    delete(actions);
                }
            }

        private:
            T **m_visitorActions;
        };

    public:

        template<typename T>
        ClassVisitorActions<T> &GetClassVisitor()
        {
            static ClassVisitorActions<T> *cached = nullptr;
            if (!cached)
            {
                cached = new ClassVisitorActions<T>;
                *cached = T::CreateClassVisitor();
                m_cleanupActions.emplace_back(new VisitorActionsHolder<ClassVisitorActions<T>>(&cached));
            }
            return *cached;
        }

    private:
        std::vector<std::unique_ptr<IHolder>> m_cleanupActions;
    };

    class IResolvedAction
    {
    public:
        virtual ~IResolvedAction() {}
        virtual IResolvedAction *GetNext() const = 0;
        virtual void Execute(class ResolvedActionContext &) = 0;
    };


    template<typename T>
    class ResolvedAction : public IResolvedAction
    {
    public:
        using ActionVecType = std::vector<typename ClassVisitorActions<std::remove_const_t<T>>::ActTyp>;

        ResolvedAction(T &instance, unsigned int currentAction, const ActionVecType &actions)
            : m_instance(instance)
            , m_currentAction(currentAction)
            , m_actions(actions)
        {
        }

        IResolvedAction *GetNext() const override
        {
            if (m_actions.size() == 0)
            {
                return nullptr;
            }

            if (m_currentAction == m_actions.size() - 1)
            {
                return nullptr;
            }

            return new ResolvedAction<T>(m_instance, m_currentAction + 1, m_actions);
        }

        void Execute(ResolvedActionContext &ctx) override
        {
            ExecuteImpl(ctx);
        }

    private:
        template<typename CtxType>
        void ExecuteImpl(CtxType &ctx)
        {
            if (m_actions.size() > 0)
            {
                m_actions[m_currentAction].Visit(m_instance, ctx);
            }
        }

        T                   &m_instance;
        const unsigned int   m_currentAction;
        const ActionVecType &m_actions;
    };

    template<typename T>
    class ResolvedCollectionAction : public IResolvedAction
    {
    public:
        using ActionVecType = std::vector<typename ClassVisitorActions<std::remove_const_t<T>>::ActTyp>;

        ResolvedCollectionAction(
            size_t               instanceCount,
            T                   *instances,
            unsigned int         currentAction,
            const ActionVecType &actions)
            : m_instanceCount(instanceCount)
            , m_instances(instances)
            , m_currentAction(currentAction)
            , m_actions(actions)
        {
            assert(m_instanceCount != 0);
        }

        IResolvedAction *GetNext() const override
        {
            if (m_actions.size() == 0 || m_currentAction == m_actions.size() - 1)
            {
                size_t nextInstanceCount = m_instanceCount - 1;
                if (nextInstanceCount == 0)
                {
                    return nullptr;
                }

                return new ResolvedCollectionAction(
                    nextInstanceCount,
                    m_instances + 1,
                    0,
                    m_actions);
            }

            return new ResolvedCollectionAction(
                m_instanceCount,
                m_instances,
                m_currentAction + 1,
                m_actions);
        }

        void Execute(ResolvedActionContext &ctx) override
        {
            ExecuteImpl(ctx);
        }

    private:
        template<typename CtxType>
        void ExecuteImpl(CtxType &ctx)
        {
            if (m_actions.size() > 0)
            {
                m_actions[m_currentAction].Visit(*m_instances, ctx);
            }
        }

        const size_t         m_instanceCount;
        T                   *m_instances;
        const unsigned int   m_currentAction;
        const ActionVecType &m_actions;
    };

    template<typename DoNextType>
    class DoNextAction : public IResolvedAction
    {
    public:
        DoNextAction(DoNextType doNext)
            : m_doNext{ doNext }
        {
        }

        IResolvedAction *GetNext() const override
        {
            return nullptr;
        }

        void Execute(ResolvedActionContext &) override
        {
            m_doNext();
        }

    private:
        DoNextType m_doNext;
    };

    class ResolvedActionContext
    {
    public:
        ResolvedActionContext()
        {
        }

        virtual ~ResolvedActionContext()
        {
            while (!m_stack.empty())
            {
                delete m_stack.top();
                m_stack.pop();
            }
        }

        template<typename T>
        void Push(T &inst)
        {
            auto& classVisitorActions = m_cache.GetClassVisitor<std::remove_const_t<T>>();
            auto& actions = classVisitorActions.GetActions();
            m_stack.push(new ResolvedAction<T>(inst, 0, actions));
        }

        template<typename T>
        void PushCollection(T *instances, size_t count)
        {
            if (count == 0) return;

            auto& classVisitorActions = m_cache.GetClassVisitor<std::remove_const_t<T>>();
            auto& actions = classVisitorActions.GetActions();
            m_stack.push(new ResolvedCollectionAction<T>(count, instances, 0, actions));
        }

        template<typename DoNextType>
        void Next(DoNextType doNext)
        {
            m_stack.push(new DoNextAction(doNext));
        }

        void Visit()
        {
            while (!m_stack.empty())
            {
                {
                    std::unique_ptr<IResolvedAction> firstAction(m_stack.top());
                    m_stack.pop();
                    IResolvedAction *nextAction = firstAction->GetNext();
                    if (nextAction)
                    {
                        m_stack.push(nextAction);
                    }
                    firstAction->Execute(*this);
                }
            }
        }

    protected:
        ClassVisitorCache             m_cache;
        std::stack<IResolvedAction *> m_stack;
    };

    class VisitorContext : public ResolvedActionContext
    {
    private:

        template<typename T, typename ContType>
        class ContinuationAction : public IResolvedAction
        {
        public:
            ContinuationAction(ContType cont)
                : m_continuation(cont)
            {
            }

            IResolvedAction *GetNext() const override
            {
                return nullptr;
            }

            void Execute(ResolvedActionContext &) override
            {
                m_continuation(m_result);
            }

            T &GetResult() const
            {
                return m_result;
            }

        private:
            T        m_result;
            ContType m_continuation;
        };

    public:
        VisitorContext(IVisitor &visitor)
            : m_visitor(visitor)
        {
        }

        ~VisitorContext()
        {
        }

        IVisitor &GetVisitor() const
        {
            return m_visitor;
        }

        template<typename T, typename ContType>
        void PushContinuation(ContType K)
        {
            ContinuationAction *contAction = new ContinuationAction<T, ContType>(K);
            m_stack.push(contAction);
            Push(contAction->GetResult());
        }

    private:
        IVisitor                     &m_visitor;
    };

    class ConstVisitorContext : public ResolvedActionContext
    {
    public:
        ConstVisitorContext(IConstVisitor &visitor)
            : m_visitor(visitor)
        {
        }

        ~ConstVisitorContext()
        {
        }

        IConstVisitor &GetVisitor() const
        {
            return m_visitor;
        }

    private:
        IConstVisitor                &m_visitor;
    };
} // namespace ATG
#pragma endregion

//--------------------------------------------------------------------------------------
// Class Visitor enables a simple, declarative specification for "visiting" a class
//--------------------------------------------------------------------------------------
#pragma region Class Visitor
namespace ATG
{
    class VisitorAdapter
    {
        template<typename EltType, size_t SIZE_>
        class IGetBufferFromArray : public IGetBuffer_t<EltType>
        {
        public:
            IGetBufferFromArray(EltType(&a)[SIZE_])
                : m_a(a)
            {
            }

            EltType *GetBuffer(size_t count) override
            {
                if (count != SIZE_)
                {
                    throw std::range_error("Wrong number of elements for fixed sized array");
                }
                return &m_a[0];
            }

        private:
            EltType(&m_a)[SIZE_];
        };

        template<typename T, typename EltType, typename Callable>
        class IGetBufferFromOwner : public IGetBuffer_t<EltType>
        {
        public:
            IGetBufferFromOwner(T &owner, Callable &callable)
                : m_owner(owner)
                , m_callable(callable)
            {
            }

            EltType *GetBuffer(size_t count) override
            {
                return m_callable(m_owner, count);
            }

        private:
            T       &m_owner;
            Callable m_callable;
        };

    public:
        VisitorAdapter(VisitorContext &ctx)
            : m_ctx(ctx)
        {
        }

        template<typename T, ENABLE_IF_INTEGRAL(T)>
        void VisitMember(T& val)
        {
            m_ctx.GetVisitor().VisitPrimitiveElement(val);
        }

        template<typename T, ENABLE_IF_NOT_INTEGRAL(T)>
        void VisitMember(T& val)
        {
            m_ctx.GetVisitor().VisitElement(m_ctx);
            m_ctx.Push(val);
        }

        template<typename EltType, size_t SIZE_, ENABLE_IF_INTEGRAL(EltType)>
        void VisitMember(EltType(&a)[SIZE_])
        {
            IGetBufferFromArray<EltType, SIZE_> getBuffer(a);
            m_ctx.GetVisitor().VisitPrimitiveCollection(getBuffer);
        }

        template<typename EltType, size_t SIZE_, ENABLE_IF_NOT_INTEGRAL(EltType)>
        void VisitMember(EltType(&a)[SIZE_])
        {
            size_t eltCount = 0;
            m_ctx.GetVisitor().VisitCollection(eltCount, m_ctx);
            if (eltCount != SIZE_)
            {
                throw std::range_error("Wrong number of elements for fixed sized array");
            }
            m_ctx.PushCollection(&a[0], SIZE_);
        }

        template<typename T, typename ValTy_, typename SetActionTy_, ENABLE_IF_INTEGRAL(ValTy_)>
        void VisitSetter(T &inst, SetActionTy_ setter)
        {
            ValTy_ val;
            m_ctx.GetVisitor().VisitPrimitiveElement(val);
            setter(inst, val);
        }

        template<typename T, typename ValTy_, typename SetActionTy_, ENABLE_IF_NOT_INTEGRAL(ValTy_)>
        void VisitSetter(T &inst, SetActionTy_ setter)
        {
            m_ctx.PushContinuation([&inst](ValTy_ &val) {
                setter(inst, val);
            });
        }

        template<typename T, typename EltType, typename Callable, ENABLE_IF_INTEGRAL(EltType)>
        void VisitCollection(T& inst, Callable setter)
        {
            IGetBufferFromOwner<T, EltType, Callable> getBuffer(inst, setter);
            m_ctx.GetVisitor().VisitPrimitiveCollection(getBuffer);
        }

        template<typename T, typename EltType, typename Callable, ENABLE_IF_NOT_INTEGRAL(EltType)>
        void VisitCollection(T& inst, Callable setter)
        {
            size_t eltCount = 0;
            m_ctx.GetVisitor().VisitCollection(eltCount, m_ctx);
            EltType *elts = setter(inst, eltCount);
            m_ctx.PushCollection(elts, eltCount);
        }

    private:
        VisitorContext &m_ctx;
    };

    class ConstVisitorAdapter
    {
    public:
        ConstVisitorAdapter(ConstVisitorContext &ctx)
            : m_ctx(ctx)
        {
        }

        template<typename T, ENABLE_IF_INTEGRAL(T)>
        void VisitMember(const T& val)
        {
            m_ctx.GetVisitor().VisitPrimitiveElement(val);
        }

        template<typename T, ENABLE_IF_NOT_INTEGRAL(T)>
        void VisitMember(const T& val)
        {
            m_ctx.GetVisitor().VisitElement(m_ctx);
            m_ctx.Push(val);
        }

        template<typename EltType, size_t SIZE_, ENABLE_IF_INTEGRAL(EltType)>
        void VisitMember(const EltType(&a)[SIZE_])
        {
            m_ctx.GetVisitor().VisitPrimitiveCollection(a, SIZE_);
        }

        template<typename EltType, size_t SIZE_, ENABLE_IF_NOT_INTEGRAL(EltType)>
        void VisitMember(const EltType(&a)[SIZE_])
        {
            m_ctx.GetVisitor().VisitCollection(SIZE_, m_ctx);
            m_ctx.PushCollection(&a[0], SIZE_);
        }

        template<typename T, typename ValTy_, typename GetActionTy_, ENABLE_IF_INTEGRAL(ValTy_)>
        void VisitGetter(const T &inst, GetActionTy_ getter)
        {
            ValTy_ val = getter(inst);
            m_ctx.GetVisitor().VisitPrimitiveElement(val);
        }

        template<typename T, typename ValTy_, typename GetActionTy_, ENABLE_IF_NOT_INTEGRAL(ValTy_)>
        void VisitGetter(const T &inst, GetActionTy_ getter)
        {
            ValTy_ val = getter(inst);
            m_ctx.Push(val);
        }

        template<typename T, typename EltType, typename Callable, ENABLE_IF_INTEGRAL(EltType)>
        void VisitCollection(const T& inst, Callable getter)
        {
            size_t count = 0;
            const EltType* elts = getter(inst, count);
            m_ctx.GetVisitor().VisitPrimitiveCollection(elts, count);
        }

        template<typename T, typename EltType, typename Callable, ENABLE_IF_NOT_INTEGRAL(EltType)>
        void VisitCollection(const T& inst, Callable getter)
        {
            size_t count = 0;
            const EltType* elts = getter(inst, count);
            m_ctx.GetVisitor().VisitCollection(count, m_ctx);
            m_ctx.PushCollection(elts, count);
        }

    private:
        ConstVisitorContext &m_ctx;
    };

    template<typename T>
    class ClassVisitorActions
    {
    private:

        class IClassVisitorActionImpl
        {
        public:
            virtual ~IClassVisitorActionImpl() {}
            virtual void VisitAction(T &inst, VisitorContext &ctx) const = 0;
            virtual void ConstVisitAction(const T &inst, ConstVisitorContext &ctx) const = 0;
        };

        template<typename HasVisitActions>
        class ImplementIClassVisitor : public HasVisitActions, public IClassVisitorActionImpl
        {
        public:
            using HasVisitActions::HasVisitActions;

            void VisitAction(T &inst, VisitorContext &ctx) const override
            {
                static_cast<const HasVisitActions*>(this)->VisitAction(inst, ctx);
            }

            void ConstVisitAction(const T &inst, ConstVisitorContext &ctx) const override
            {
                static_cast<const HasVisitActions*>(this)->ConstVisitAction(inst, ctx);
            }
        };

    public:
        template<typename TB>
        class ClassVisitorAction
        {
        public:
            template<typename MmbrTyp>
            struct P2Mmbr
            {
                typedef MmbrTyp TB::*ptr;
            };

            ClassVisitorAction(IClassVisitorActionImpl *impl)
                : m_impl(impl)
            {
            }

            ClassVisitorAction(ClassVisitorAction<TB> &&rhs) noexcept
                : m_impl(std::move(rhs.m_impl))
            {
            }

            ClassVisitorAction &operator=(ClassVisitorAction<TB> &&rhs)
            {
                m_impl = std::move(rhs.m_impl);
                return *this;
            }

            ClassVisitorAction(const ClassVisitorAction &) = delete;
            ClassVisitorAction&  operator=(const ClassVisitorAction &) = delete;

            void Visit(TB &inst, class ResolvedActionContext &ctx) const
            {
                m_impl->VisitAction(inst, static_cast<VisitorContext&>(ctx));
            }

            void Visit(const TB &inst, class ResolvedActionContext &ctx) const
            {
                m_impl->ConstVisitAction(inst, static_cast<ConstVisitorContext&>(ctx));
            }

        private:
            std::unique_ptr<IClassVisitorActionImpl> m_impl;
        };

        typedef ClassVisitorAction<T> ActTyp;

        template<typename VisitorActionType, typename... Args>
        void AddVisitorAction(Args... args)
        {
            auto action = new ImplementIClassVisitor<VisitorActionType>(args...);
            m_actions.emplace_back(std::move(action));
        }

        const std::vector<ActTyp> &GetActions() const
        {
            return m_actions;
        }

    private:
        std::vector<ActTyp> m_actions;
    };

    // Visit the class with Direct access to the IVisitor/IConstVisitor
    // This gives you the most direct access to the class and visitor
    // E.g. use this perform extra step(s) after visiting a specific class member
    // E.g. perform extra side-effects on the visitor that aren't directly tied to a single class member
    // E.g. perform extra side-effects on the class etc.
    // E.g. Establish class invariants after a destructive visit to the class
    // E.g. Utilize the visitor without consuming/modifying data from the class
    // Type Parameters:
    //    ClassType            -- Type of the class to visit
    //    ConstVisitorCallable -- Type of a callable object (e.g. function pointer, functor, lambda, etc.)
    //                            that performs side effect when this action is visited. (Does not perform
    //                            a destructive action on the class.)
    //                            E.g. void (*) (const ClassType &, IConstVisitor &);
    //    VisitorCallable      -- Type of a callable object (e.g. funciton pointer, functor, lambda, etc.)
    //                            that performs a side effect when this action is visited. (Can perform a
    //                            a destructive action on the class.)
    //                            E.g. void (*) (ClassType &, IVisitor &)
    //
    // Normal Parameters:
    //     constVisitorCallable -- Callable that consumes a const reference to the class and a reference to
    //                             the IConstVistior that is visiting the class.
    //     visitorCallable      -- Callable that consumes a (non-const) reference to the class and a reference
    //                             to the IVisitor that is visiting the class.
    template<typename ClassType, typename ConstVisitorCallable, typename VisitorCallable>
    class VisitDirectActions
    {
    public:
        VisitDirectActions(ConstVisitorCallable constVisitorCallable, VisitorCallable visitorCallable)
            : m_constVisitorCallable(constVisitorCallable)
            , m_visitorCallable(visitorCallable)
        {
        }

        void VisitAction(ClassType &inst, VisitorContext &ctx) const
        {
            m_visitorCallable(inst, ctx.GetVisitor());
        }

        void ConstVisitAction(const ClassType &inst, ConstVisitorContext &ctx) const
        {
            m_constVisitorCallable(inst, ctx.GetVisitor());
        }

    private:
        ConstVisitorCallable m_constVisitorCallable;
        VisitorCallable      m_visitorCallable;
    };

    template<typename ClassType, typename ConstVisitorCallable, typename VisitorCallable>
    void VisitDirect(ClassVisitorActions<ClassType> &actions, ConstVisitorCallable constVisitorCallable, VisitorCallable visitorCallable)
    {
        actions.template AddVisitorAction<VisitDirectActions<ClassType, ConstVisitorCallable, VisitorCallable>>(constVisitorCallable, visitorCallable);
    }

    // Visit a class member
    // Type Parameters:
    //    ClassType -- Type of the class whose member will be visited
    //    MmbrType  -- Type of the member to visit
    //
    // Normal Parameters:
    //    mbr -- pointer-to-member for the class member to visit
    template<typename ClassType, typename MmbrType>
    class VisitMemberAction
    {
    public:
        VisitMemberAction(MmbrType ClassType::*mbr)
            : m_mbr(mbr)
        {
        }

        void VisitAction(ClassType &inst, VisitorContext &ctx) const
        {
            VisitorAdapter(ctx).VisitMember(inst.*m_mbr);
        }

        void ConstVisitAction(const ClassType &inst, ConstVisitorContext &ctx) const
        {
            ConstVisitorAdapter(ctx).VisitMember(inst.*m_mbr);
        }

    private:
        MmbrType ClassType::*m_mbr;
    };

    template<typename ClassType, typename MmbrType>
    void VisitMember(ClassVisitorActions<ClassType> &actions, MmbrType ClassType::*mbr)
    {
        actions.template AddVisitorAction<VisitMemberAction<ClassType, MmbrType>>(mbr);
    }

    // Visit a collection of elements that is pointed to by a unique_ptr
    // Type Parameters:
    //    ClassType        -- Type of the class that owns the unique_ptr.
    //    EltType          -- Type of the elements pointed to by the unique_ptr.
    //    CountType        -- Type of the class member that stores the element count.
    //
    // Normal Parameters:
    //    UUP          -- pointer-to-member of type unique_ptr. The unique_ptr member points to the elements that will be visited.
    //    count        -- pointer-to-member that holds the count of objects in the collection.
    template<typename ClassType, typename EltType, typename CountType>
    class VisitUniquePointerCollectionAction
    {
    public:
        VisitUniquePointerCollectionAction(std::unique_ptr<EltType> ClassType::*UPP, CountType ClassType::*count)
            : m_UPP(UPP)
            , m_count(count)
        {}

        void VisitAction(ClassType &inst, VisitorContext &ctx) const
        {
            VisitorAdapter(ctx).VisitCollection<ClassType, EltType>(inst, *this);
        }

        void ConstVisitAction(const ClassType &inst, ConstVisitorContext &ctx) const
        {
            ConstVisitorAdapter(ctx).VisitCollection<ClassType, EltType>(inst, *this);
        }

        EltType *operator()(ClassType &inst, size_t eltCount)
        {
            auto& UP = inst.*m_UPP;
            inst.*m_count = CountType(eltCount);
            UP.reset(new EltType[eltCount]);
            return UP.get();
        }

        const EltType *operator()(const ClassType &inst, size_t &eltCount)
        {
            eltCount = inst.*m_count;
            auto& UP = inst.*m_UPP;
            return UP.get();
        }

    private:
        std::unique_ptr<EltType> ClassType::*m_UPP;
        CountType ClassType::*m_count;
    };

    template<typename ClassType, typename EltType, typename CountType>
    void VisitUniquePointerCollection(ClassVisitorActions<ClassType> &actions, std::unique_ptr<EltType> ClassType::*UPP, CountType ClassType::*count)
    {
        actions.template AddVisitorAction<VisitUniquePointerCollectionAction<ClassType, EltType, CountType>>(UPP, count);
    }

    // Visit one or zero elements pointed to by a unique_ptr
    // Type Parameters:
    //    ClassType        -- Type of the class that owns the unique_ptr.
    //    EltType          -- Type of the elements pointed to by the unique_ptr.
    //
    // Normal Parameters:
    //    UUP          -- pointer-to-member of type unique_ptr. The unique_ptr member points to the elements that will be visited.
    template<typename ClassType, typename EltType>
    class VisitNullableUniquePtrAction
    {
    public:
        VisitNullableUniquePtrAction(std::unique_ptr<EltType> ClassType::*UPP)
            : m_UPP(UPP)
        {
        }

        void VisitAction(ClassType &inst, VisitorContext &ctx) const override
        {
            VisitorAdapter(ctx).VisitCollection<ClassType, EltType>(inst, *this);
        }

        void ConstVisitAction(const ClassType &inst, ConstVisitorContext & ctx) const override
        {
            ConstVisitorAdapter(ctx).VisitCollection<ClassType, EltType>(inst, *this);
        }

        EltType *operator()(ClassType &inst, size_t eltCount)
        {
            auto& UP = inst.*m_UPP;
            if (eltCount == 0)
            {
                UP = nullptr;
            }
            else
            {
                assert(eltCount == 1);
                UP.reset(new EltType);
            }
            return UP.get();
        }

        const EltType *operator()(const ClassType &inst, size_t &eltCount)
        {
            auto& UP = inst.*m_UPP;
            auto result = UP.get();
            eltCount = result == nullptr ? 0 : 1;
            return result;
        }

    private:
        std::unique_ptr<EltType> ClassType::*m_UPP;
    };

    template<typename ClassType, typename EltType>
    void VisitNullableUniquePointer(ClassVisitorActions<ClassType> &actions, std::unique_ptr<EltType> ClassType::*UPP)
    {
        actions.AddVisitorAction<VisitNullableUniquePtrAction<ClassType, EltType>>(UPP);
    }

    // Visit a collection of elements contained in a std::vector
    // Type Parameters:
    //    ClassType        -- Type of the class that owns the vector.
    //    EltType          -- Type of the elements contained in the vector.
    //
    // Normal Parameters:
    //    VecP         -- pointer-to-member of type vector that contains the elements that will be visited.
    template<typename ClassType, typename EltType>
    class VisitVectorCollectionAction
    {
    public:
        VisitVectorCollectionAction(std::vector<EltType> ClassType::*VecP)
            : m_VecP(VecP)
        {
        }

        void VisitAction(ClassType &inst, VisitorContext &ctx) const
        {
            VisitorAdapter(ctx).VisitCollection<ClassType, EltType>(inst, *this);
        }

        void ConstVisitAction(const ClassType &inst, ConstVisitorContext & ctx) const
        {
            ConstVisitorAdapter(ctx).VisitCollection<ClassType, EltType>(inst, *this);
        }

        EltType *operator()(ClassType &inst, size_t eltCount)
        {
            auto& vec = inst.*m_VecP;
            vec.clear();
            vec.resize(eltCount);
            return eltCount ? &vec[0] : nullptr;
        }

        const EltType *operator()(const ClassType &inst, size_t &eltCount)
        {
            auto& vec = inst.*m_VecP;
            eltCount = vec.size();
            return eltCount ? &vec[0] : nullptr;
        }

    private:
        std::vector<EltType> ClassType::*m_VecP;
    };

    template<typename ClassType, typename EltType>
    void VisitVectorCollection(ClassVisitorActions<ClassType> &actions, std::vector<EltType> ClassType::*VecP)
    {
        actions.template AddVisitorAction<VisitVectorCollectionAction<ClassType, EltType>>(VecP);
    }

    // Visit a collection of chars contained in a std::string
    // Type Parameters:
    //    ClassType         -- Type of the class that owns the string.
    // Normal Parameters:
    //    VecP         -- pointer-to-member of type string that will be visited.
    template<typename ClassType>
    class VisitStringAction
    {
    public:
        VisitStringAction(std::string ClassType::*StrP)
            : m_StrP(StrP)
        {
        }

        void VisitAction(ClassType &inst, VisitorContext &ctx) const
        {
            VisitorAdapter(ctx).VisitCollection<ClassType, char>(inst, *this);
        }

        void ConstVisitAction(const ClassType &inst, ConstVisitorContext &ctx) const
        {
            ConstVisitorAdapter(ctx).VisitCollection<ClassType, char>(inst, *this);
        }

        char *operator()(ClassType &inst, size_t eltCount)
        {
            auto& str = inst.*m_StrP;
            str.clear();
            str.resize(eltCount);
            return &str[0];
        }

        const char *operator()(const ClassType &inst, size_t &eltCount)
        {
            auto& str = inst.*m_StrP;
            eltCount = str.size();
            return str.c_str();
        }

    private:
        std::string ClassType::*m_StrP;
    };

    template<typename ClassType>
    void VisitString(ClassVisitorActions<ClassType> &actions, std::string ClassType::*StrP)
    {
        actions.template AddVisitorAction<VisitStringAction<ClassType>>(StrP);
    }

    // Visit a collection of elements using functions to get and set the collection elements
    // Type Parameters:
    //    ClassType         -- Type of the class that owns the collection.
    //    EltType           -- Type of the elements contained in the collection.
    //    ConstEltsCallable -- Type of callable object (e.g.  function pointer, functor, lambda, etc.) to retrieve
    //                         the size of the collection as well as a pointer to the collection elements.
    //                         E.g. const EltType *(*)(const ClassType &, size_t &)
    //                         Notice that it returns a pointer to the elements and modifies the size_t argument
    //    EltsCallable      -- Type of callable object (e.g. function pointer, functor, lambda, etc.) to retrieve
    //                         a pointer to an array of new elements of the requested size.
    //                         E.g. EltType *(*)(ClasType&, size_t)
    //                         Notice that it returns a pointer to the memory containg the new elements and consumes
    //                         the count of new elements located in the memory location
    //
    // Normal Parameters:
    //   constEltsGetter -- Callable to retrieve the size of the collection as well as a pointer to the colleciton elements
    //   eltsSetter      -- Callable to retrieve a pointer to memory containing the requested count of new elements
    template<typename ClassType, typename EltType, typename ConstEltsCallable, typename EltsCallable>
    class VisitCollectionWithFunctionsAction
    {
    public:
        VisitCollectionWithFunctionsAction(ConstEltsCallable constEltsGetter, EltsCallable eltsSetter)
            : m_constEltsGetter(constEltsGetter)
            , m_eltsSetter(eltsSetter)
        {}

        void VisitAction(ClassType &inst, VisitorContext &ctx) const override
        {
            VisitorAdapter(ctx).VisitCollection<ClassType, EltType>(inst, m_eltsSetter);
        }

        void ConstVisitAction(const ClassType &inst, ConstVisitorContext &ctx) const override
        {
            ConstVisitorAdapter(ctx).VisitCollection<ClassType, EltType>(inst, m_constEltsGetter);
        }

    private:
        ConstEltsCallable m_constEltsGetter;
        EltsCallable      m_eltsSetter;
    };

    template<typename ClassType, typename EltType, typename ConstEltsCallable, typename EltsCallable>
    void VisitCollectionWithFunctions(ClassVisitorActions<ClassType> &actions, ConstEltsCallable constEltsGetter, EltsCallable eltsSetter)
    {
        actions.AddVisitorAction<VisitCollectionWithFunctionsAction<ClassType, EltType, ConstEltsCallable, EltsCallable>>(constEltsGetter, eltsSetter);
    }

    // Visit a class member using a getter function and setter function
    // Type Paramerts
    //    ClassType     -- type of the class to visit
    //    EltType       -- type of the element retrieved and set byt the getter and setter
    //    GetActionType -- callable for getting a value of type EltType from the instance of ClassType
    //    SetActionType -- callable for setting a value of type EltType within the instance of ClassType
    //
    // Normal Parameters:
    //    getter -- get a value of type EltType from the class instance
    //    setter -- set a avlue of type EltType within the class instance
    template<typename ClassType, typename EltType, typename GetActionType, typename SetActionType>
    class VisitGetterSetterAction
    {
    public:

        VisitGetterSetterAction(GetActionType getter, SetActionType setter)
            : m_getter(getter)
            , m_setter(setter)
        {
        }

        void VisitAction(ClassType &inst, VisitorContext &ctx) const
        {
            VisitorAdapter(ctx).VisitSetter<ClassType, EltType, SetActionType>(inst, m_setter);
        }

        void ConstVisitAction(const ClassType &inst, ConstVisitorContext & ctx) const
        {
            ConstVisitorAdapter(ctx).VisitGetter<ClassType, EltType, GetActionType>(inst, m_getter);
        }

    private:
        GetActionType m_getter;
        SetActionType m_setter;
    };

    template<typename ClassType, typename EltType, typename GetActionType, typename SetActionType>
    void VisitGetterSetter(ClassVisitorActions<ClassType> &actions, GetActionType getter, SetActionType setter)
    {
        actions.template AddVisitorAction<VisitGetterSetterAction<ClassType, EltType, GetActionType, SetActionType>>(getter, setter);
    }

    // Template function to create a class visitor for your class
    // By default, it assumes that you have a static CreateClassVisitor method
    // however, you can specialize this function in the case when you dont want
    // to or cannot add a static method to your class.
    template<typename T>
    ClassVisitorActions<T> CreateClassVisitor()
    {
        return T::CreateClassVisitor();
    }

} // namespace ATG
#pragma endregion

//--------------------------------------------------------------------------------------
// Serialization
// Implements serialization using the generic visitor implementation
//--------------------------------------------------------------------------------------
#pragma region Serialization
namespace ATG
{
    class FixedSizeSerializationBuffer
    {
    public:
        FixedSizeSerializationBuffer(uint8_t *outputBuffer, size_t outputBufferSize)
            : m_bytesWritten(0)
            , m_outputBufferSize(outputBufferSize)
            , m_outputBuffer(outputBuffer)
        {
        }

        size_t GetBytesWritten() const
        {
            return m_bytesWritten;
        }

        template<typename T>
        void WriteIntegers(const T *vals, size_t count)
        {
            size_t bytesRemaining = m_outputBufferSize - m_bytesWritten;
            size_t requiredSize = sizeof(T) * count;
            if (bytesRemaining < requiredSize)
            {
                throw std::overflow_error("Output buffer is too small to receive the expected data.");
            }
            if(count > 0)
                memcpy_s(&m_outputBuffer[m_bytesWritten], bytesRemaining, vals, requiredSize);
            m_bytesWritten += requiredSize;
        }

    private:
        size_t             m_bytesWritten;
        const size_t       m_outputBufferSize;
        uint8_t           *m_outputBuffer;
    };

    class StreamSerializationBuffer
    {
    public:
        StreamSerializationBuffer(std::basic_ostream<char> &strm)
            : m_bytesWritten(0)
            , m_stream(strm)
        {
        }

        size_t GetBytesWritten() const
        {
            return m_bytesWritten;
        }

        template<typename T>
        void WriteIntegers(const T *vals, size_t count)
        {
            size_t requiredSize = sizeof(T) * count;
            m_stream.write(reinterpret_cast<const char*>(vals), requiredSize);
            m_bytesWritten += requiredSize;
        }

    private:
        size_t                    m_bytesWritten;
        std::basic_ostream<char> &m_stream;
    };

    class VectorSerializationBuffer
    {
    public:
        VectorSerializationBuffer(std::vector<uint8_t> &buffer)
            : m_bytesWritten(0)
            , m_buffer(buffer)
        {
        }

        size_t GetBytesWritten() const
        {
            return m_bytesWritten;
        }

        template<typename T>
        void WriteIntegers(const T *vals, size_t count)
        {
            size_t requiredSize = sizeof(T) * count;
            m_buffer.resize(m_bytesWritten + requiredSize);
            if(count > 0)
                memcpy_s(&m_buffer[m_bytesWritten], requiredSize, vals, requiredSize);
            m_bytesWritten += requiredSize;
        }

    private:
        size_t                m_bytesWritten;
        std::vector<uint8_t> &m_buffer;
    };

    // --------------------------------------------------------------------------------
    //  Serializing to Output
    // --------------------------------------------------------------------------------
    template<typename buffer_t>
    class Serializer : public ImplementIConstVisitor<Serializer<buffer_t>>
    {
    public:
        Serializer(buffer_t &serializationBuffer)
            : m_serializationBuffer(serializationBuffer)
        {
        }

        // Serialize a simple integral type
        template<typename T>
        void ImplementVisitPrimitiveElement(T val)
        {
            m_serializationBuffer.WriteIntegers(&val, 1);
        }

        template<typename EltType>
        void ImplementVisitPrimitiveCollection(const EltType *elts, size_t count)
        {
            // Serialize the element count
            ImplementVisitPrimitiveElement(count);

            // Serialize the elements
            m_serializationBuffer.WriteIntegers(elts, count);
        }

        void ImplementVisitCollection(size_t count, class ConstVisitorContext &)
        {
            // Serialize the element count
            ImplementVisitPrimitiveElement(count);
        }

        void ImplementVisitElement(class ConstVisitorContext &)
        {
        }

    private:
        buffer_t &m_serializationBuffer;
    };

    template<typename T, typename buffer_t>
    size_t Serialize(const T &serializeMe, buffer_t &srzBffr)
    {
        Serializer<buffer_t> srzr(srzBffr);
        ConstVisitorContext ctx(srzr);
        ctx.Push(serializeMe);
        ctx.Visit();
        return srzBffr.GetBytesWritten();
    }

    template<typename T>
    size_t Serialize(const T &serializeMe, uint8_t *outputBuffer, size_t outputBufferSize)
    {
        FixedSizeSerializationBuffer srzBffr(outputBuffer, outputBufferSize);
        return Serialize(serializeMe, srzBffr);
    }

} // namespace ATG
#pragma endregion

//--------------------------------------------------------------------------------------
// Deserialization
// Implements deserialization using the generic visitor implementation
//--------------------------------------------------------------------------------------
#pragma region Deserialization
namespace ATG
{
    class FixedSizeDeserializationBuffer
    {
    public:
        FixedSizeDeserializationBuffer(const uint8_t *inputBuffer, size_t inputBufferSize)
            : m_bytesRead(0)
            , m_inputBufferSize(inputBufferSize)
            , m_inputBuffer(inputBuffer)
        {
        }

        size_t GetBytesRead() const
        {
            return m_bytesRead;
        }

        template<typename T>
        void ReadIntegers(T *vals, size_t count)
        {
            size_t bytesRemaining = m_inputBufferSize - m_bytesRead;
            size_t requiredSize = sizeof(T) * count;
            if (bytesRemaining < requiredSize)
            {
                throw std::overflow_error("Input buffer is too small to contain the expected data.");
            }
            memcpy_s(vals, requiredSize, &m_inputBuffer[m_bytesRead], requiredSize);
            m_bytesRead += requiredSize;
        }

    private:
        size_t             m_bytesRead;
        const size_t       m_inputBufferSize;
        const uint8_t     *m_inputBuffer;
    };

    class StreamDeserializationBuffer
    {
    public:
        StreamDeserializationBuffer(std::basic_istream<char> &strm)
            : m_bytesRead(0)
            , m_stream(strm)
        {
        }

        size_t GetBytesRead() const
        {
            return m_bytesRead;
        }

        template<typename T>
        void ReadIntegers(T *vals, size_t count)
        {
            size_t requiredSize = sizeof(T) * count;
            m_stream.read(reinterpret_cast<char*>(vals), requiredSize);
            m_bytesRead += requiredSize;
        }

    private:
        size_t                    m_bytesRead;
        std::basic_istream<char> &m_stream;
    };

    // --------------------------------------------------------------------------------
    //  Deserializing from Input
    // --------------------------------------------------------------------------------
    template<typename buffer_t>
    class Deserializer : public ImplementIVisitor<Deserializer<buffer_t>>
    {
    public:
        Deserializer(buffer_t &deserializationBuffer)
            : m_deserializationBuffer(deserializationBuffer)
        {
        }

        // Deserialize a simple integral type
        template<typename T>
        void ImplementVisitPrimitiveElement(T &val)
        {
            m_deserializationBuffer.ReadIntegers(&val, 1);
        }

        // Deserialize a collection of elements of integral type
        template<typename EltType>
        void ImplementVisitPrimitiveCollection(IGetBuffer_t<EltType> &getBuffer)
        {
            // Deserialize the size
            size_t eltCount = 0;
            ImplementVisitPrimitiveElement(eltCount);

            EltType *elts = getBuffer.GetBuffer(eltCount);

            // Deserialize the elements
            m_deserializationBuffer.ReadIntegers(elts, eltCount);
        }

        void ImplementVisitCollection(size_t &count, class VisitorContext &)
        {
            // Deserialize the element count
            ImplementVisitPrimitiveElement(count);
        }

        void ImplementVisitElement(class VisitorContext &)
        {
        }

    private:
        buffer_t &m_deserializationBuffer;
    };

    template<typename T, typename buffer_t>
    size_t Deserialize(T &deserializeMe, buffer_t &dsrzBffr)
    {
        Deserializer<buffer_t> dsrzr(dsrzBffr);
        VisitorContext ctx(dsrzr);
        ctx.Push(deserializeMe);
        ctx.Visit();
        return dsrzBffr.GetBytesRead();
    }

    template<typename T>
    size_t Deserialize(T &deserializeMe, const uint8_t *inputBuffer, size_t inputBufferSize)
    {
        FixedSizeDeserializationBuffer dsrzBffr(inputBuffer, inputBufferSize);
        return Deserialize(deserializeMe, dsrzBffr);
    }

} // namespace ATG
#pragma endregion


//--------------------------------------------------------------------------------------
// Serialization Header
// File header to be serialized/deserialized to track the serialization version and
// endianness
//--------------------------------------------------------------------------------------
#pragma region Serialization Header
namespace ATG
{
#define SERIALIZATION_CURRENT_VERSION_STRING "v0.1"

    class SerializationHeader
    {
    public:
        enum SerializationFlags : uint32_t
        {
            none = 0,
            is_host_endian = 1,
            is_current_version = 2,
        };

        static ClassVisitorActions<SerializationHeader> CreateClassVisitor()
        {
            ClassVisitorActions<SerializationHeader> actions;

            // Serialize the version string
            VisitString(actions, &SerializationHeader::m_verString);

            // Set the version flag based on the version string
            VisitDirect<SerializationHeader>(actions,
                [](const SerializationHeader &, IConstVisitor&) {},
                [](SerializationHeader &header, IVisitor&)
            {
                uint32_t flags = static_cast<uint32_t>(header.m_flags);
                constexpr uint32_t isCurrentVersion = static_cast<uint32_t>(SerializationHeader::SerializationFlags::is_current_version);

                if (header.m_verString == SERIALIZATION_CURRENT_VERSION_STRING)
                {
                    flags |= isCurrentVersion;
                }
                else
                {
                    flags &= ~isCurrentVersion;
                }

                header.m_flags = static_cast<SerializationHeader::SerializationFlags>(flags);
            });

            // Serialize/Deserialize a byte order mark and adjust the flags accordingly
            VisitGetterSetter<SerializationHeader, wchar_t>(
                actions,
                [](const SerializationHeader &)
            {
                return wchar_t(0xFEFF); // Deliver the byte-order-mark to the serializer
            },
                [](SerializationHeader &header, wchar_t BOM)
            {
                // Consume the byte-order-mark from the visitor and then adjust the flags accordingly
                uint32_t flags = static_cast<uint32_t>(header.m_flags);
                constexpr uint32_t isHostEndianFlag = static_cast<uint32_t>(SerializationHeader::SerializationFlags::is_host_endian);
                switch (BOM)
                {
                case 0xFEFF:
                    flags |= isHostEndianFlag; // Bytes occur in host order
                    break;

                case 0xFFFE:
                    flags &= ~isHostEndianFlag; // Bytes are swapped (not host order)
                    break;

                default:
                    throw std::invalid_argument("Unrecognized byte-order-mark in serialization stream");
                }
                header.m_flags = static_cast<SerializationHeader::SerializationFlags>(flags);
            });

            return actions;
        }

        SerializationHeader()
            : m_verString(SERIALIZATION_CURRENT_VERSION_STRING)
            , m_flags(SerializationFlags::none)
        {
        }

        const char *GetVersion() const
        {
            return m_verString.c_str();
        }

        SerializationFlags GetFlags() const
        {
            return m_flags;
        }

        bool CheckFlag(SerializationFlags flag) const
        {
            const uint32_t flagVal = static_cast<uint32_t>(flag);
            const uint32_t flagsVal = static_cast<uint32_t>(m_flags);

            return ((flagsVal & flagVal) == flagVal);
        }

    private:
        std::string        m_verString;
        SerializationFlags m_flags;
    };

} // namespace ATG
#pragma endregion
