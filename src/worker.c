/**
 *    author:     UncP
 *    date:    2018-08-24
 *    license:    BSD-3
**/

#include <assert.h>

#include "worker.h"

worker* new_worker(uint32_t id, uint32_t total, barrier *b)
{
  assert(id < total);

  worker *w = (worker *)malloc(sizeof(worker));
  w->id = id;
  w->total = total;
  w->bar = b;

  w->max_path = 64;
  w->cur_path = 0;
  w->paths = (path *)malloc(sizeof(path) * w->max_path);

  w->max_fence = 4;
  w->cur_fence = 0;
  w->fences = (fence *)malloc(sizeof(fence) * w->max_fence);

  w->prev = 0;
  w->next = 0;

  return w;
}

void free_worker(worker* w)
{
  free((void *)w->fences);
  free((void *)w->paths);
  free((void *)w);
}

path* worker_get_new_path(worker *w)
{
  if (w->cur_path == w->max_path) {
    w->max_path = (uint32_t)((float)w->max_path * 1.5);
    w->paths = (path *)realloc(w->paths, sizeof(path) * w->max_path);
  }
  assert(w->cur_path < w->max_path);
  return &w->paths[w->cur_path++];
}

path* worker_get_path_at(worker *w, uint32_t idx)
{
  assert(w->cur_path > idx);
  return &w->paths[idx];
}

uint32_t worker_get_path_count(worker *w)
{
  return w->cur_path;
}

fence* worker_get_new_fence(worker *w)
{
  if (w->cur_fence == w->max_fence) {
    w->max_fence = (uint32_t)((float)w->max_fence * 1.5);
    w->fences = (fence *)realloc(w->fences, sizeof(fence) * w->max_fence);
  }
  assert(w->cur_path < w->max_path);
  return &w->fences[w->cur_fence++];
}

fence* worker_get_fence_at(worker *w, uint32_t idx)
{
  assert(w->cur_fence > idx);
  return &w->fences[idx];
}

uint32_t worker_get_fence_count(worker *w)
{
  return w->cur_fence;
}

void worker_clear(worker *w)
{
  // TODO: optimize ???
  for (uint32_t i = 0; i < w->max_path; ++i)
    path_clear(&w->paths[i]);
  w->cur_path = 0;

  for (uint32_t i = 0; i < w->max_fence; ++i)
    w->fences[i].len = 0;
  w->cur_fence = 0;
}

void worker_link(worker *a, worker *b)
{
  a->next = b;
  b->prev = a;
}
