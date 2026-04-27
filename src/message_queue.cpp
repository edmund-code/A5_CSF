#include <iostream>
#include <ctime>
#include "guard.h"
#include "message_queue.h"

MessageQueue::MessageQueue() {
  sem_init(&m_sem, 0, 0);
  pthread_mutex_init(&m_lock, nullptr);
}

MessageQueue::~MessageQueue() {
  sem_destroy(&m_sem);
  pthread_mutex_destroy(&m_lock);
}

void MessageQueue::enqueue(std::shared_ptr<Message> msg) {
  {
    Guard g(m_lock);
    m_queue.push_back(msg);
  }
  sem_post(&m_sem);
}

std::shared_ptr<Message> MessageQueue::dequeue() {
  //wait up to 1 sec for a message
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += 1;

  if (sem_timedwait(&m_sem, &ts) != 0) {
    //timeout or error
    return std::shared_ptr<Message>();
  }

  Guard g(m_lock);
  std::shared_ptr<Message> msg = m_queue.front();
  m_queue.pop_front();
  return msg;
}
