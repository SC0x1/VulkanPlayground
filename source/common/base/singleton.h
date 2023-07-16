#pragma once

#include <stdint.h>

template<typename T>
class Singleton
{
public:
    static T* CreateInstance()
    {
        if (ms_singletonInstances == 0u)
        {
            ms_instance = new T();
        }
        ++ms_singletonInstances;
        return ms_instance;
    }

    static T* AssignInstance(T* instance)
    {
        ms_instance = instance;
        return ms_instance;
    }

    static void DestroyInstance()
    {
        --ms_singletonInstances;
        if (ms_singletonInstances == 0u)
        {
            delete(ms_instance);
        }
    }
    static T* GetInstance() { return ms_instance; }

protected:
    inline Singleton();
    inline ~Singleton();
    T& operator=(const T&) = delete;
private:
    static T* ms_instance;
    static uint32_t ms_singletonInstances;
};

template <typename T>
T* Singleton<T>::ms_instance = nullptr;

template <typename T>
uint32_t Singleton<T>::ms_singletonInstances = 0u;

template<typename T>
Singleton<T>::Singleton()
{
    ms_instance = static_cast<T*>(this);
}

template<typename T>
Singleton<T>::~Singleton()
{
    ms_instance = nullptr;
}


//
// Declare singleton methods (this is not thread safe)
//  Usage:
//      QB_SINGLETON_GENERIC_METHODS(, CLASSNAME);
//      QB_SINGLETON_GENERIC_METHODS(DLL_API_NAME, CLASSNAME);
//
#define QB_SINGLETON_GENERIC_METHODS(_export, __CLASSNAME__)                  \
    protected:                                                                \
        __CLASSNAME__ ();                                                     \
        __CLASSNAME__& operator=(const __CLASSNAME__&) = delete;              \
        virtual ~__CLASSNAME__();                                             \
    private:                                                                  \
        _export static __CLASSNAME__* ms_instance;                            \
        static uint32_t ms_singletonInstances;                                \
    public:                                                                   \
        _export static __CLASSNAME__* CreateInstance()                        \
        {                                                                     \
            if(ms_singletonInstances == 0u)                                   \
            {                                                                 \
                ms_instance = new __CLASSNAME__();                            \
            }                                                                 \
            ++ms_singletonInstances;                                          \
            return ms_instance;                                               \
        }                                                                     \
        _export static __CLASSNAME__* AssignInstance(__CLASSNAME__* instance) \
        {                                                                     \
            ms_instance = instance;                                           \
            return ms_instance;                                               \
        }                                                                     \
        _export static bool HasInstance()                                     \
        {                                                                     \
            return ms_instance == nullptr;                                    \
        }                                                                     \
        _export static void DestroyInstance()                                 \
        {                                                                     \
            --ms_singletonInstances;                                          \
            if (ms_singletonInstances == 0u)                                  \
            {                                                                 \
                delete(ms_instance);                                          \
            }                                                                 \
        }                                                                     \
        _export static __CLASSNAME__* GetInstance() { return ms_instance; }


#define QB_SINGLETON_GENERIC_DEFINE(__CLASSNAME__)       \
    __CLASSNAME__* __CLASSNAME__::ms_instance = nullptr; \
    uint32_t __CLASSNAME__::ms_singletonInstances = 0u;     

#define QB_SINGLETON_GENERIC_DEFINE_TEMPLATE(__TEMPLATE__, __CLASSNAME__) \
    __TEMPLATE__ __CLASSNAME__* __CLASSNAME__::ms_instance = nullptr;     \
    __TEMPLATE__ uint32_t __CLASSNAME__::ms_singletonInstances = 0u;
