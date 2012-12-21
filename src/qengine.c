/*
 * See Copyright Notice in qnode.h
 */

#include <stdio.h>
#include "qassert.h"
#include "qengine.h"
#include "qlog.h"
#include "qmalloc.h"

#define qRETIRED_FD -1

extern const struct qdispatcher_t epoll_dispatcher;

static void init_qevent(qevent_t *event) {
  event->fd = qRETIRED_FD;
  event->flags = 0;
  event->read = event->write = NULL;
  event->data = NULL;
}

qengine_t* qengine_new() {
  qengine_t *engine = qalloc_type(qengine_t);
  engine->max_fd = 0;
  engine->dispatcher = &(epoll_dispatcher);
  if (engine->dispatcher->init(engine) < 0) {
    qfree(engine);
    return NULL;
  }
  engine->events = qalloc_array(qevent_t, QMAX_EVENTS);
  engine->active_events = qalloc_array(qevent_t, QMAX_EVENTS);
  int i = 0;
  for (i = 0; i < QMAX_EVENTS; ++i) {
    qevent_t *event = &(engine->events[i]);
    init_qevent(event);
    event = &(engine->active_events[i]);
    init_qevent(event);
  }
  return engine;
}

int qengine_add_event(qengine_t *engine, int fd, int flags, qevent_func_t *callback, void *data) {
  if (fd > QMAX_EVENTS) {
    qerror("extends max fd");
    return -1;
  }
  qevent_t *event = &(engine->events[fd]);
  if (engine->dispatcher->add(engine, fd, flags) < 0) {
    qerror("add event error");
    return -1;
  }
  event->fd = fd;
  event->flags |= flags;
  event->data = data;
  if (flags & QEVENT_READ) {
    event->read = callback;
  }
  if (flags & QEVENT_WRITE) {
    event->write = callback;
  }
  if (fd > engine->max_fd) {
    engine->max_fd = fd;
  }
  return 0;
}

int qengine_del_event(qengine_t* engine, int fd, int flags) {
  if (fd > QMAX_EVENTS) {
    qerror("extends max fd");
    return -1;
  }
  if (flags == QEVENT_NONE) {
    return -1;
  }
  qevent_t *event = &(engine->events[fd]);
  event->flags = event->flags & (~flags);
  if (fd == engine->max_fd && event->flags == QEVENT_NONE) {
    int i;
    for (i = engine->max_fd - 1; i > 0; --i) {
      if (engine->events[i].flags != QEVENT_NONE) {
        engine->max_fd = i;
        break;
      }
    }
  }
  engine->dispatcher->del(engine, fd, flags);
  return 0;
}

int qengine_loop(qengine_t* engine) {
  int done = 0;
  int num, i;
  struct timeval time;
  while (!done) {
    time.tv_sec = time.tv_usec = 0;
    num = engine->dispatcher->poll(engine, NULL);
    for (i = 0; i < num; i++) {
      qevent_t *event = &(engine->events[engine->active_events[i].fd]);
      int flags = engine->active_events[i].flags;
      int fd = engine->active_events[i].fd;
      int read = 0;

      if (event->flags & flags & QEVENT_READ) {
        read = 1;
        event->read(fd, flags, event->data);
      }
      if (event->flags & flags & QEVENT_WRITE) {
        if (!read || event->write != event->read) {
          event->write(fd, flags, event->data);
        }
      }
    }
  }
  return 0;
}

void qengine_destroy(qengine_t *engine) {
}

int qengine_add_timer(qengine_t* engine, struct timeval *time, int flags) {
  return 0;
}

int qengine_del_timer(qengine_t* engine, int id) {
  return 0;
}
