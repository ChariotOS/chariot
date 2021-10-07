#include <ck/ipc.h>
#include <stdlib.h>
#include <sys/sysbind.h>
#include <chariot/mshare.h>
#include <chariot.h>
#include <sched.h>
#include "chariot/ck/memory_order.h"
#include "unistd.h"
#include <ck/time.h>
#include <errno.h>
#include <sys/syscall.h>

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))


#ifdef IPC_USE_SHM
static void *mshare_create(const char *name, size_t size) {
  struct mshare_create arg;
  arg.size = size;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);
  return (void *)sysbind_mshare(MSHARE_CREATE, &arg);
}


static void *mshare_acquire(const char *name, size_t size) {
  struct mshare_acquire arg;
  arg.size = size;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);
  return (void *)sysbind_mshare(MSHARE_ACQUIRE, &arg);
}
#endif


ck::ipc::impl::socket_connection::socket_connection(ck::ref<ck::IPCSocket> s, const char *ns, bool is_server)
    : m_sock(move(s)), ns(ns), is_server(is_server) {
  assert(m_sock->connected());

  handshake();
  m_recv_fiber_join = ck::spawn([this] {
    // This is just the maximum size of a message in the IPC protocol :)
    constexpr size_t receive_buffer_size = 1024 * 32;
    void *recv_buf = malloc(max_message_size);
    while (!closed()) {
      // printf("%s reading...\n", side());
      auto recv_size = m_sock->async_recv(recv_buf, max_message_size, MSG_DONTWAIT).await();

      // printf("%s got something:  %d!\n", side(), recv_size);
      if (recv_size <= 0) {
        // printf("%s: closed!\n", side());
        close();
        break;
      } else {
        // ck::hexdump(recv_buf, recv_size);
        dispatch(recv_buf, recv_size);
      }
    }


    free(recv_buf);
  });
}



void ck::ipc::impl::socket_connection::handshake(void) {
  if (is_server) {
    int x = 42;
    if (m_sock->send(&x, sizeof(x), 0) < 0) close();
  } else {
    int x = 0;
    if (m_sock->recv(&x, sizeof(x), 0) < 0) close();
    assert(x == 42);
  }
}

ck::ipc::encoder ck::ipc::impl::socket_connection::begin_send(void) {
  if (msg_buffer == NULL) msg_buffer = malloc(max_message_size);
  msg_size = 0;
  return ck::ipc::encoder(msg_buffer, msg_size);

  // ... continued in finish_send
}


void ck::ipc::impl::socket_connection::finish_send(void) {
  if (closed()) return;

  if (m_sock->send(msg_buffer, msg_size, 0) <= 0) {
    close();
    return;
  }
}



struct msg_header {
  uint32_t type;
  ck::ipc::nonce_t nonce;
};

void ck::ipc::impl::socket_connection::dispatch(void *data, size_t len) {
  // was consumed by sync_wait
  if (data == NULL) return;

  assert(len >= sizeof(msg_header));

  // real quick check if the message has a nonce we are looking for
  auto *header = (struct msg_header *)data;


  if (sync_wait_futures.contains(header->nonce)) {
    auto f = sync_wait_futures.get(header->nonce);
    f.resolve(ck::ipc::raw_msg{data, len});
  } else {
    dispatch_received_message(data, len);
  }
}


ck::ipc::raw_msg ck::ipc::impl::socket_connection::sync_wait(uint32_t msg_type, ck::ipc::nonce_t nonce) {
  if (closed()) return ck::ipc::raw_msg{};
  // printf("%d sync wait on msg of type %d with a nonce of %d\n", is_server, msg_type, nonce);
  ck::ipc::raw_msg res = {nullptr, 0};

  // TODO: thread safety!

  ck::future<raw_msg> fut;
  // add the future to the map of nonces
  sync_wait_futures[nonce] = fut;

  auto [data, sz] = fut.await();
  res.data = data;
  res.sz = sz;
  sync_wait_futures.remove(nonce);

  return res;
}
