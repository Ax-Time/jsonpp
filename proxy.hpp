#ifndef PROXY_HPP
#define PROXY_HPP

#include <memory>
#include <type_traits>
#include <iostream>

template <typename T>
class Proxy
{
    /**
     * The Proxy class is a wrapper around an object T.
     * It is useful to keep a shared reference to the same object of class T across multiple instances of Proxy.
     * It provides a transparent interface to the object T.
     */
    static_assert(std::is_default_constructible_v<T>, "T must be default constructible");
    static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");
    // Must have clone method
    static_assert(std::is_same_v<decltype(std::declval<T>().clone()), T *>, "T must have clone method");

private:
    std::shared_ptr<std::shared_ptr<T>> pp;

public:
    /**
     * The Proxy class constructor. It passes the arguments to the constructor of T.
     */
    template <typename... Args>
    Proxy(Args... args) : pp(std::make_shared<std::shared_ptr<T>>(std::make_shared<T>(args...))) {}
    /**
     * It creates a new Proxy from a shared pointer to an object of class U, which must be a subclass of T.
     */
    template <typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>, typename... Args>
    Proxy(std::shared_ptr<U> up) : pp(std::make_shared<std::shared_ptr<T>>(static_cast<std::shared_ptr<T>>(up))) {}
    /**
     * The Proxy class copy constructor. It creates a new Proxy instance that shares the same object of class T.
     */
    Proxy(const Proxy<T> &other) : pp(other.pp) {}
    /**
     * It constructs a new object of class T and makes all copies of this proxy object reference the new object.
     */
    template <typename... Args>
    void reset(Args... args) { *pp = std::make_shared<T>(args...); }
    /**
     * It makes all copies of this proxy object reference the same object as the other proxy object.
     */
    void reset(const Proxy<T> &other) { *pp = *other.pp; }
    /**
     * It returns a reference to the underlying object of class T.
     */
    T &operator*() const { return **pp; }
    /**
     * It returns a pointer to the underlying object of class T.
     */
    T *operator->() const { return (*pp).get(); }
    /**
     * It returns a pointer to the underlying object of class T, casted to class U.
     */
    template <typename U>
    U *as() const { return reinterpret_cast<U *>((*pp).get()); }
    /**
     * It returns a new Proxy instance that references a copy of the underlying object of class T.
     */
    Proxy<T> clone() const
    {
        T cloned = *((*pp)->clone());
        return Proxy<T>(cloned);
    }
    friend std::ostream &operator<<(std::ostream &os, const Proxy<T> &proxy)
    {
        os << **proxy.pp;
        return os;
    }
};

#endif
