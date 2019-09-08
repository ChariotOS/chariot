#pragma once

#include <vec.h>
#include <lock.h>

class fifo_buf {
public:
    fifo_buf()
        : m_write_buffer(&m_buffer1)
        , m_read_buffer(&m_buffer2)
    {
    }

    ssize_t write(const u8*, ssize_t);
    ssize_t read(u8*, ssize_t);

    bool is_empty() const { return m_empty; }

    // FIXME: Isn't this racy? What if we get interrupted between getting the buffer pointer and dereferencing it?
    ssize_t bytes_in_write_buffer() const { return (ssize_t)m_write_buffer->size(); }

private:
    void flip();
    void compute_emptiness();

    vec<u8>* m_write_buffer { nullptr };
    vec<u8>* m_read_buffer { nullptr };
    vec<u8> m_buffer1;
    vec<u8> m_buffer2;
    ssize_t m_read_buffer_index = 0;
    bool m_empty = true;
    mutex_lock m_lock;
};
