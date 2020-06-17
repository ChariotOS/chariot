/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// obviously stolen from Serenity :)

#pragma once

#include <chariot.h>

namespace ck {

class intrusive_list_node;
class intrusive_list_storage {
private:
    friend class intrusive_list_node;
    template<class T, intrusive_list_node T::*member>
    friend class intrusive_list;
    intrusive_list_node* m_first { nullptr };
    intrusive_list_node* m_last { nullptr };
};

template<class T, intrusive_list_node T::*member>
class intrusive_list {
public:
    intrusive_list();
    ~intrusive_list();

    void clear();
    bool is_empty() const;
    void append(T& n);
    void prepend(T& n);
    void remove(T& n);
    bool contains(const T&) const;
    T* first() const;
    T* last() const;

    T* take_first();

    class Iterator {
    public:
        Iterator();
        Iterator(T* value);

        T& operator*() const;
        T* operator->() const;
        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const { return !(*this == other); }
        Iterator& operator++();
        Iterator& erase();

    private:
        T* m_value { nullptr };
    };

    Iterator begin();
    Iterator end();

private:
    static T* next(T* current);
    static T* node_to_value(intrusive_list_node& node);
    intrusive_list_storage m_storage;
};

class intrusive_list_node {
public:
    ~intrusive_list_node();
    void remove();
    bool is_in_list() const;

private:
    template<class T, intrusive_list_node T::*member>
    friend class intrusive_list;
    intrusive_list_storage* m_storage = nullptr;
    intrusive_list_node* m_next = nullptr;
    intrusive_list_node* m_prev = nullptr;
};

template<class T, intrusive_list_node T::*member>
inline intrusive_list<T, member>::Iterator::Iterator()
{
}

template<class T, intrusive_list_node T::*member>
inline intrusive_list<T, member>::Iterator::Iterator(T* value)
    : m_value(value)
{
}

template<class T, intrusive_list_node T::*member>
inline T& intrusive_list<T, member>::Iterator::operator*() const
{
    return *m_value;
}

template<class T, intrusive_list_node T::*member>
inline T* intrusive_list<T, member>::Iterator::operator->() const
{
    return m_value;
}

template<class T, intrusive_list_node T::*member>
inline bool intrusive_list<T, member>::Iterator::operator==(const Iterator& other) const
{
    return other.m_value == m_value;
}

template<class T, intrusive_list_node T::*member>
inline typename intrusive_list<T, member>::Iterator& intrusive_list<T, member>::Iterator::operator++()
{
    m_value = intrusive_list<T, member>::next(m_value);
    return *this;
}

template<class T, intrusive_list_node T::*member>
inline typename intrusive_list<T, member>::Iterator& intrusive_list<T, member>::Iterator::erase()
{
    T* old = m_value;
    m_value = intrusive_list<T, member>::next(m_value);
    (old->*member).remove();
    return *this;
}

template<class T, intrusive_list_node T::*member>
inline intrusive_list<T, member>::intrusive_list()

{
}

template<class T, intrusive_list_node T::*member>
inline intrusive_list<T, member>::~intrusive_list()
{
    clear();
}

template<class T, intrusive_list_node T::*member>
inline void intrusive_list<T, member>::clear()
{
    while (m_storage.m_first)
        m_storage.m_first->remove();
}

template<class T, intrusive_list_node T::*member>
inline bool intrusive_list<T, member>::is_empty() const
{
    return m_storage.m_first == nullptr;
}

template<class T, intrusive_list_node T::*member>
inline void intrusive_list<T, member>::append(T& n)
{
    auto& nnode = n.*member;
    if (nnode.m_storage)
        nnode.remove();

    nnode.m_storage = &m_storage;
    nnode.m_prev = m_storage.m_last;
    nnode.m_next = nullptr;

    if (m_storage.m_last)
        m_storage.m_last->m_next = &nnode;
    m_storage.m_last = &nnode;
    if (!m_storage.m_first)
        m_storage.m_first = &nnode;
}

template<class T, intrusive_list_node T::*member>
inline void intrusive_list<T, member>::prepend(T& n)
{
    auto& nnode = n.*member;
    if (nnode.m_storage)
        nnode.remove();

    nnode.m_storage = &m_storage;
    nnode.m_prev = nullptr;
    nnode.m_next = m_storage.m_first;

    if (m_storage.m_first)
        m_storage.m_first->m_prev = &nnode;
    m_storage.m_first = &nnode;
    if (!m_storage.m_last)
        m_storage.m_last = &nnode;
}

template<class T, intrusive_list_node T::*member>
inline void intrusive_list<T, member>::remove(T& n)
{
    auto& nnode = n.*member;
    if (nnode.m_storage)
        nnode.remove();
}

template<class T, intrusive_list_node T::*member>
inline bool intrusive_list<T, member>::contains(const T& n) const
{
    auto& nnode = n.*member;
    return nnode.m_storage == &m_storage;
}

template<class T, intrusive_list_node T::*member>
inline T* intrusive_list<T, member>::first() const
{
    return m_storage.m_first ? node_to_value(*m_storage.m_first) : nullptr;
}

template<class T, intrusive_list_node T::*member>
inline T* intrusive_list<T, member>::take_first()
{
    if (auto* ptr = first()) {
        remove(*ptr);
        return ptr;
    }
    return nullptr;
}

template<class T, intrusive_list_node T::*member>
inline T* intrusive_list<T, member>::last() const
{
    return m_storage.m_last ? node_to_value(*m_storage.m_last) : nullptr;
}

template<class T, intrusive_list_node T::*member>
inline T* intrusive_list<T, member>::next(T* current)
{
    auto& nextnode = (current->*member).m_next;
    T* nextstruct = nextnode ? node_to_value(*nextnode) : nullptr;
    return nextstruct;
}

template<class T, intrusive_list_node T::*member>
inline typename intrusive_list<T, member>::Iterator intrusive_list<T, member>::begin()
{
    return m_storage.m_first ? Iterator(node_to_value(*m_storage.m_first)) : Iterator();
}

template<class T, intrusive_list_node T::*member>
inline typename intrusive_list<T, member>::Iterator intrusive_list<T, member>::end()
{
    return Iterator();
}

template<class T, intrusive_list_node T::*member>
inline T* intrusive_list<T, member>::node_to_value(intrusive_list_node& node)
{
    return (T*)((char*)&node - ((char*)&(((T*)nullptr)->*member) - (char*)nullptr));
}

inline intrusive_list_node::~intrusive_list_node()
{
    if (m_storage)
        remove();
}

inline void intrusive_list_node::remove()
{
    assert(m_storage);
    if (m_storage->m_first == this)
        m_storage->m_first = m_next;
    if (m_storage->m_last == this)
        m_storage->m_last = m_prev;
    if (m_prev)
        m_prev->m_next = m_next;
    if (m_next)
        m_next->m_prev = m_prev;
    m_prev = nullptr;
    m_next = nullptr;
    m_storage = nullptr;
}

inline bool intrusive_list_node::is_in_list() const
{
    return m_storage != nullptr;
}

}
