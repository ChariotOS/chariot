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


ck::ipc::impl::socket_connection::socket_connection(
    ck::ref<ck::ipcsocket> s, const char *ns, bool is_server)
    : m_sock(move(s)), ns(ns), is_server(is_server) {
  assert(m_sock->connected());

  handshake();
  if (!closed()) {
    m_sock->on_read([this] {
      this->drain_messages();
      this->dispatch();
    });
  }
}



void ck::ipc::impl::socket_connection::handshake(void) {
#ifdef IPC_USE_SHM
  if (is_server) {
    msg_handshake hs;
    hs.total_shared_msg_queue_size = total_shared_msg_queue_size;
    int range = '~' - ' ';
    for (int i = 0; i < sizeof(hs.shm_name) - 1; i++) {
      hs.shm_name[i] = rand() % range + ' ';
    }
    hs.shm_name[sizeof(hs.shm_name) - 1] = '\0';

    shm_name = hs.shm_name;
    shared_msg_queue = mshare_create(shm_name.get(), total_shared_msg_queue_size);

    if (m_sock->send(&hs, sizeof(hs), 0) < 0) {
      close();
      return;
    }

  } else {
    msg_handshake hs;
    if (m_sock->recv(&hs, sizeof(hs), 0) < 0) {
      close();
      return;
    }
    shm_name = hs.shm_name;
    total_shared_msg_queue_size = hs.total_shared_msg_queue_size;

    shared_msg_queue = mshare_acquire(shm_name.get(), total_shared_msg_queue_size);
  }

  struct shared_msg_queue *pos0 = (struct shared_msg_queue *)shared_msg_queue;
  struct shared_msg_queue *pos1 =
      (struct shared_msg_queue *)((char *)shared_msg_queue + total_shared_msg_queue_size / 2);

  if (is_server) {
    recv_queue = pos0;
    send_queue = pos1;
  } else {
    recv_queue = pos1;
    send_queue = pos0;
  }


  // each side initializes their own recv queue
  pthread_mutex_init(&recv_queue->lock, NULL);

#endif
}

ck::ipc::encoder ck::ipc::impl::socket_connection::begin_send(void) {
#ifdef IPC_USE_SHM
  // step 1, lock the mutex
  // printf("s: grabbing lock\n");
  pthread_mutex_lock(&send_queue->lock);
  // printf("s: got lock\n");

  // step 2, find a free slot in the queue
  auto *msg = send_queue->data;
  for (; msg->size != 0; msg = (struct shared_msg_data *)(msg->data + round_up(msg->size, 8))) {
  }
  // return the encoder with that slot
  return ck::ipc::encoder(msg->data, msg->size);

#else

  if (msg_buffer == NULL) msg_buffer = malloc(32 * 1024);

  msg_size = 0;

  return ck::ipc::encoder(msg_buffer, msg_size);

#endif

  // ... continued in finish_send
}


void ck::ipc::impl::socket_connection::finish_send(void) {
  if (closed()) return;

#ifdef IPC_USE_SHM
  // printf("finish send\n");
  // step 3, increment the count in the queue
  // the encoder wrote directly to the queue, so no need to copy or
  // anything, just bang on the doorbell
  bool hit_doorbell = send_queue->count == 0;

  send_queue->count += 1;

  uint8_t doorbell_msg = 0xFF;
  if (m_sock->send(&doorbell_msg, 1, 0) <= 0) {
    close();
    return;
  }

  // step 5, unlock the mutex
  // printf("s: releasing lock\n");
  pthread_mutex_unlock(&send_queue->lock);

#else

  if (m_sock->send(msg_buffer, msg_size, 0) <= 0) {
    close();
    return;
  }

#endif
}



void ck::ipc::impl::socket_connection::drain_messages(void) {
  if (!m_sock) return;

#ifdef IPC_USE_SHM
  pthread_mutex_lock(&recv_queue->lock);

  if (recv_queue->count > 0) {
    auto *msg = recv_queue->data;
    for (int i = 0; i < recv_queue->count; i++) {
      void *data = malloc(msg->size);
      memcpy(data, msg->data, msg->size);
      stored_messages.push({data, msg->size});

      auto newmsg = (struct shared_msg_data *)(msg->data + round_up(msg->size, 8));
      // clear out the old msg size just in case
      msg->size = 0;
      msg = newmsg;
    }
    recv_queue->count = 0;
  }

  uint8_t doorbell_msg = 0x00;
  int nread = m_sock->recv(&doorbell_msg, 1, MSG_DONTWAIT);
  if (nread < 0) {
    close();
  }

  pthread_mutex_unlock(&recv_queue->lock);

#else

  while (1) {
    uint8_t buf;
    ssize_t sz = m_sock->recv(&buf, 1, MSG_IPC_QUERY | MSG_DONTWAIT);

    if (sz < 0 || m_sock->eof()) {
      if (errno == EAGAIN) break;

      close();
      break;
    }

    char *buffer = (char *)malloc(sz);
    int nread = m_sock->recv(buffer, sz, 0);
    if (nread != sz || m_sock->eof()) {
      free(buffer);
      close();
      break;
    }
    stored_messages.push({buffer, (size_t)sz});
  }
#endif
}

struct msg_header {
  uint32_t type;
  ck::ipc::nonce_t nonce;
};

void ck::ipc::impl::socket_connection::dispatch(void) {
  if (stored_messages.size() > 0) {
    auto v = stored_messages;
    stored_messages.clear();


    for (auto &m : v) {
      // was consumed by sync_wait
      if (m.data == NULL) continue;

      assert(m.sz >= sizeof(msg_header));

      // real quick check if the message has a nonce we are looking for
      auto *header = (struct msg_header *)m.data;


      if (sync_wait_futures.contains(header->nonce)) {
        auto f = sync_wait_futures.get(header->nonce);
        f.resolve(move(m));
      } else {
        dispatch_received_message(m.data, m.sz);
        free(m.data);
      }
    }
  }
}


static int awaitfs(struct await_target *fds, int nfds, int flags, long long timeout_time) {
  int res = 0;
  do {
    res = sysbind_awaitfs(fds, nfds, flags, timeout_time);
  } while (res == -EINTR);
  return errno_wrap(res);
}

ck::ipc::raw_msg ck::ipc::impl::socket_connection::sync_wait(
    uint32_t msg_type, ck::ipc::nonce_t nonce) {
  // printf("%d sync wait on msg of type %d with a nonce of %d\n", is_server, msg_type, nonce);
  ck::ipc::raw_msg res = {nullptr, 0};
  bool found = false;

  while (1) {
    // double check we don't have the message waiting for us already
    for (auto &m : stored_messages) {
      if (m.data == NULL) continue;
      uint32_t type = *(uint32_t *)m.data;
      // printf("%d sitting on msg of type %d\n", is_server, type);

      // if the type is the one we want, that means it will have a nonce
      if (type == msg_type) {
        ck::ipc::nonce_t sent_nonce = *(ck::ipc::nonce_t *)(&((uint32_t *)m.data)[1]);
        // printf("%d nonce = %d, want %d\n", is_server, sent_nonce, nonce);
        // ck::hexdump(m.data, m.len);
        if (sent_nonce == nonce) {
          res.data = m.data;
          res.sz = m.sz;
          m.data = NULL;
          found = true;

          break;
        }
      }
    }

    if (found) break;


    // because the ipc mechanism might not be run in an environment with fibers (just in case),
    // we need to check if we are in one before we do anything with futures.
    if (ck::fiber::in_fiber()) {
      m_lock.lock();

      ck::future<raw_msg> fut;
      // add the future to the map of nonces
      sync_wait_futures[nonce] = fut;
      // release the lock once we inserted the future into the map
      m_lock.unlock();

      auto [data, sz] = fut.await();
      res.data = data;
      res.sz = sz;

      // grab the lock and remove the future from the map
      m_lock.lock();
      sync_wait_futures.remove(nonce);
      m_lock.unlock();

      found = true;
      break;
    } else {
      // dispatch on the messages that we have but don't need right now
      // (this ought to allow some kind of reentrancy)
      dispatch();

      if (closed()) {
        // returning NULL from here means we're dead :^)
        res = {nullptr, 0};
        break;
      }

      // TODO: awaitfs is slow lol. we should be able to do this without it

      int await_res = 1;

      struct await_target targ;
      // m_sock is alive, as we previously checked if we were dead :)
      targ.fd = m_sock->fileno();
      targ.awaiting = AWAITFS_READ;
      await_res = awaitfs(&targ, 1, 0, -1);

      drain_messages();
      // m_lock.unlock();
    }
  }

  return res;
}
