#include <ck/ipc.h>
#include <stdlib.h>
#include <sys/sysbind.h>
#include <chariot/mshare.h>

void *mshare_create(const char *name, size_t size) {
  struct mshare_create arg;
  arg.size = size;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);
  return (void *)sysbind_mshare(MSHARE_CREATE, &arg);
}


void *mshare_acquire(const char *name, size_t size) {
  struct mshare_acquire arg;
  arg.size = size;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);
  return (void *)sysbind_mshare(MSHARE_ACQUIRE, &arg);
}

ck::ipc::impl::socket_connection::socket_connection(
    ck::ref<ck::ipcsocket> s, const char *ns, bool is_server)
    : m_sock(move(s)), ns(ns), is_server(is_server) {
  assert(m_sock->connected());

  handshake();
  m_sock->on_read([this] { this->drain_messages(); });
}


void ck::ipc::impl::socket_connection::handshake(void) {
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

    m_sock->send(&hs, sizeof(hs), 0);

  } else {
    msg_handshake hs;
    m_sock->recv(&hs, sizeof(hs), 0);
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
}

ck::ipc::encoder ck::ipc::impl::socket_connection::begin_send(void) {
  // step 1, lock the mutex
  pthread_mutex_lock(&send_queue->lock);

  // step 2, find a free slot in the queue
  auto *msg = send_queue->data;
  for (; msg->size != 0; msg = (struct shared_msg_data *)(msg->data + msg->size)) {
  }
  // return the encoder with that slot
  return ck::ipc::encoder(msg->data, msg->size);

  // .. continued in finish_send
}


void ck::ipc::impl::socket_connection::finish_send(void) {
  // step 3, increment the count in the queue
  // the encoder wrote directly to the queue, so no need to copy or
  // anything, just bang on the doorbell
  bool hit_doorbell = send_queue->count == 0;

  send_queue->count += 1;

  // step 4, hit the doorbell
  if (hit_doorbell) {
    uint8_t doorbell_msg = 0xFF;
    m_sock->send(&doorbell_msg, 1, MSG_DONTWAIT);
  }

  // step 5, unlock the mutex
  pthread_mutex_unlock(&send_queue->lock);
}



void ck::ipc::impl::socket_connection::drain_messages(void) {
  pthread_mutex_lock(&recv_queue->lock);


  //   printf("there are %d messages\n", recv_queue->count);
  if (recv_queue->count > 0) {
    auto *msg = recv_queue->data;
    for (int i = 0; i < recv_queue->count; i++) {
      this->dispatch_received_message(msg->data, msg->size);

      auto newmsg = (struct shared_msg_data *)(msg->data + msg->size);
      // clear out the old msg size just in case

      //   memset(msg, 0, sizeof(struct shared_msg_data) + msg->size);
      msg->size = 0;
      msg = newmsg;
    }

    recv_queue->count = 0;
  }

  uint8_t doorbell_msg = 0x00;
  int nread = m_sock->recv(&doorbell_msg, 1, MSG_IPC_CLEAR | MSG_DONTWAIT);


  pthread_mutex_unlock(&recv_queue->lock);


  //   free(buffer);
}