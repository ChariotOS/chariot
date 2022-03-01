/*
 * This file is part of the Hawkbeans JVM developed by
 * the HExSA Lab at Illinois Institute of Technology.
 *
 * Copyright (c) 2017, Kyle C. Hale <khale@cs.iit.edu>
 *
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the
 * file "LICENSE.txt".
 */
#include <class.h>
#include <gc.h>
#include <hashtable.h>
#include <list.h>
#include <mm.h>
#include <stack.h>
#include <stdlib.h>
#include <string.h>
#include <thread.h>
#include <time.h>

/*
 * This implements a somewhat-precise mark-and-sweep collector
 * for Hawkbeans. Note that it's not *actually* precise since
 * we don't actually know what a reference looks like. We get
 * precision by storing every reference we allocated in a reference
 * table (a hash table). Clearly inefficient.
 *
 * Root Set: - Base object (and static fields of its class)
 * 	     - Base thread's frame (including locals and op stack)
 *
 * When an object is allocated from the *managed* heap,
 * a reference is also allocated from the C *unmanaged* heap.
 * The reference is inserted by address into the ref table, and its
 * entry status is set to PRESENT.
 *
 * In the Mark phase, the ref table is scanned, and all entries have
 * their status reset to ABSENT.
 *
 * We then scan the roots, and perform lookups on the ref table. This
 * is currently done on every candidate, which is dumb. If found,
 * the ref table entry is set back to PRESENT.
 *
 * In the Sweep phase, we scan the table again. All ABSENT entries
 * are garbage. We find the object on the heap and buddy_free() it,
 * and we subsequently free() the ref table entry and its reference.
 *
 */

extern jthread_t* cur_thread;

/*
 * Scan all the root nodes that have been registered
 * with the GC.
 *
 */
static int scan_roots(gc_state_t* state) {
  gc_root_t* root;

  list_for_each_entry(root, &state->root_list, link) {
    GC_DEBUG("Scanning root (%s)", root->name);

    if (root->scan(state, root->ptr) != 0) {
      HB_ERR("Could not scan root (%s)", root->name);
      return -1;
    }
  }

  return 0;
}

/*
 * Insert a reference into the ref table.
 *
 */
static int ref_tbl_insert_ref(obj_ref_t* ref) {
  gc_state_t* state = cur_thread->gc_state;
  ref_entry_t* entry = NULL;

  entry = malloc(sizeof(ref_entry_t));

  if (!entry) {
    HB_ERR("Could not allocate ref entry");
    return -1;
  }

  memset(entry, 0, sizeof(ref_entry_t));

  entry->state = GC_REF_PRESENT;

  if (nk_htable_insert(state->ref_tbl->htable, (unsigned long)ref,
                       (unsigned long)entry) == 0) {
    HB_ERR("Could not insert ref entry");
    return -1;
  }

  return 0;
}

/*
 * Remove a reference from the ref table.
 *
 */
static int ref_tbl_remove_ref(obj_ref_t* ref) {
  ref_entry_t* entry = NULL;
  gc_state_t* state = cur_thread->gc_state;

  entry = (ref_entry_t*)nk_htable_remove(state->ref_tbl->htable,
                                         (unsigned long)ref, 0);

  if (!entry) {
    HB_ERR("No ref entry to remove");
    return -1;
  }

  free(entry);

  return 0;
}

/*
 * Set all entries in the reference table to not present
 * (GC_REF_ABSENT)
 *
 */
static int clear_ref_tbl(gc_state_t* state) {
  struct nk_hashtable_iter* iter =
      nk_create_htable_iter(state->ref_tbl->htable);

  if (!iter) {
    HB_ERR("Could not create ref table iterator in %s", __func__);
    return -1;
  }

  do {
    ref_entry_t* entry = (ref_entry_t*)nk_htable_get_iter_value(iter);

    if (entry) {
      entry->state = GC_REF_ABSENT;
    }

  } while (nk_htable_iter_advance(iter) != 0);

  nk_destroy_htable_iter(iter);

  return 0;
}

/*
 * Mark phase of the GC. Clear the ref table (set
 * all its entries to not present), then
 * begin a scan of the heap beginning at root nodes. Live
 * object refs will be set back to present, preventing
 * their collection by the GC in the sweep phase.
 *
 */
static int mark(gc_state_t* state) {
  GC_DEBUG("BEGIN MARK PHASE");

  if (clear_ref_tbl(state) != 0) {
    HB_ERR("Could not clear ref table");
    return -1;
  }

  // scan roots will reset the status for live refs
  if (scan_roots(state) != 0) {
    HB_ERR("Could not scan roots");
    return -1;
  }

  return 0;
}

/*
 * Wrapper for array allocation. Allocates
 * an array on the heap and inserts its reference
 * into the ref table.
 *
 */
obj_ref_t* gc_array_alloc(u1 type, i4 count) {
  obj_ref_t* ref = array_alloc(type, count);

  if (!ref) {
    HB_ERR("GC could not allocate array object");
    return NULL;
  }

  if (ref_tbl_insert_ref(ref) != 0) {
    return NULL;
  }

  return ref;
}

/*
 * Wrapper for String object allocation.
 * Allocates a String object on the heap
 * and inserts its reference in the ref table.
 *
 */
obj_ref_t* gc_str_obj_alloc(const char* str) {
  obj_ref_t* ref = string_object_alloc(str);

  if (!ref) {
    HB_ERR("GC could not allocate string object");
    return NULL;
  }

  if (ref_tbl_insert_ref(ref) != 0) {
    return NULL;
  }

  return ref;
}

/*
 * Wrapper for object allocation. Allocates an
 * object then inserts its reference in the
 * ref table.
 *
 */
obj_ref_t* gc_obj_alloc(java_class_t* cls) {
  obj_ref_t* ref = object_alloc(cls);

  if (!ref) {
    HB_ERR("GC could not allocate object");
    return NULL;
  }

  if (ref_tbl_insert_ref(ref) != 0) {
    return NULL;
  }

  return ref;
}

/*
 * Sweeps the reference table, collecting
 * any dead refs and the objects they point
 * to on the heap.
 *
 */
// WRITE ME
static int sweep(gc_state_t* state) {
  struct nk_hashtable_iter* iter =
      nk_create_htable_iter(state->ref_tbl->htable);

  if (!iter) {
    HB_ERR("Could not create ref table iterator in %s", __func__);
    return -1;
  }
  int live = 0;

  do {
    ref_entry_t* entry = (ref_entry_t*)nk_htable_get_iter_value(iter);

    if (entry && entry->state == GC_REF_ABSENT) {
      // delete the reference table entry for this object
      obj_ref_t* ref = (obj_ref_t*)nk_htable_get_iter_key(iter);

      native_obj_t* nobj = (native_obj_t*)ref->heap_ptr;

      int size = 1 << nobj->order;

      ref_tbl_remove_ref(ref);
      object_free((native_obj_t*)ref->heap_ptr);
      free(ref);

      state->collect_stats.obj_collected++;
      state->collect_stats.bytes_reclaimed += size;
    } else if (entry) {
      live++;
    }

  } while (nk_htable_iter_advance(iter) != 0);


  nk_destroy_htable_iter(iter);
  return 0;
}

static unsigned ref_hash_fn(unsigned long key) {
  return nk_hash_long(key, sizeof(unsigned long) * 8);
}

static int ref_eq_fn(unsigned long k1, unsigned long k2) { return (k1 == k2); }

/*
 * Initializes the reference tracking table
 *
 */
static int init_ref_tbl(gc_state_t* state) {
  state->ref_tbl = malloc(sizeof(gc_ref_tbl_t));
  if (!state->ref_tbl) {
    HB_ERR("Could not allocate ref table");
    return -1;
  }
  memset(state->ref_tbl, 0, sizeof(gc_ref_tbl_t));

  state->ref_tbl->htable = nk_create_htable(0, ref_hash_fn, ref_eq_fn);

  if (!state->ref_tbl->htable) {
    HB_ERR("Could not creater ref hashtable");
    return -1;
  }

  return 0;
}

/*
 * creates a GC root struct and adds it to
 * the root list
 *
 */
static int add_root(void* root_ptr,
                    int (*scan_fn)(gc_state_t* gc_state, void* priv_data),
                    const char* name, gc_state_t* state) {
  gc_root_t* root = NULL;

  if (!root_ptr) {
    HB_ERR("Given bad root (%s) in %s", name, __func__);
    return -1;
  }

  root = malloc(sizeof(gc_root_t));

  if (!root) {
    HB_ERR("Could not allocate root struct");
    return -1;
  }

  memset(root, 0, sizeof(gc_root_t));

  root->ptr = root_ptr;
  root->scan = scan_fn;
  root->name = name;

  // add it to the root list
  list_add(&root->link, &state->root_list);

  GC_DEBUG("GC added root (%s)", name);

  return 0;
}

/*
 * Scan the base object reference (always
 * sets it to present). This is the reference to
 * the object for the class passed in at the command
 * line
 *
 */
static int scan_base_ref(gc_state_t* gc_state, void* priv_data) {
  obj_ref_t* ref = (obj_ref_t*)priv_data;
  ref_entry_t* ref_ent = NULL;

  ref_ent = (ref_entry_t*)nk_htable_search(gc_state->ref_tbl->htable,
                                           (unsigned long)ref);

  if (!ref_ent) {
    HB_ERR("Could not find base object reference in hash!");
    return -1;
  }

  ref_ent->state = GC_REF_PRESENT;

  return 0;
}

/*
 * If this is a valid reference (i.e.,
 * it exists in the reference table),
 * return 1, otherwise 0
 *
 */
static inline int is_valid_ref(obj_ref_t* ref, gc_state_t* state) {
  return nk_htable_search(state->ref_tbl->htable, (unsigned long)ref) != 0;
}

/*
 * mark a reference, but also return if it was already marked
 */
static int remark_ref(obj_ref_t* ref, gc_state_t* state) {
  ref_entry_t* entry = NULL;

  entry = (ref_entry_t*)nk_htable_search(state->ref_tbl->htable,
                                         (unsigned long)ref);

  if (!entry) {
    HB_ERR("GC could not mark non-present ref");
    return -1;
  }
  int was = entry->state;

  entry->state = GC_REF_PRESENT;

  return was == GC_REF_PRESENT;
}

/*
 * Mark a reference in the reference table as
 * present (alive).
 *
 */
static int mark_ref(obj_ref_t* ref, gc_state_t* state) {
  ref_entry_t* entry = NULL;

  entry = (ref_entry_t*)nk_htable_search(state->ref_tbl->htable,
                                         (unsigned long)ref);

  if (!entry) {
    HB_ERR("GC could not mark non-present ref");
    return -1;
  }

  entry->state = GC_REF_PRESENT;

  return 0;
}

/*
 * look at all fields of the base object
 *
 * Also look at its class static fields
 */
// WRITE ME
static int scan_base_obj(gc_state_t* gc_state, void* priv_data) {
  // pull the object from the private data
  native_obj_t* obj = (native_obj_t*)priv_data;
  // pull the class as well

  // 1) look at all the fields in the base object
  for (int i = 0; i < obj->field_count; i++) {
    var_t field = obj->fields[i];
    if (is_valid_ref(field.obj, gc_state)) {
      if (!remark_ref(field.obj, gc_state)) {
        scan_base_obj(gc_state, (void*)field.obj->heap_ptr);
      }
    }
  }
  // Not totally sure we need to check the static fields of this, since we do
  // that when we scan the classmap
  return 0;
}

/*
 * Scan stack frames
 */
// WRITE ME
static int scan_base_frame(gc_state_t* gc_state, void* priv_data) {
  // walk over every stack frame, mark objects and scan them as a base
  struct stack_frame* frame = priv_data;

  for (stack_frame_t* stk = frame; frame != NULL; frame = frame->next) {
    for (int i = 0; i < stk->max_locals; i++) {
      var_t local = stk->locals[i];
      if (is_valid_ref(local.obj, gc_state)) {
        if (!remark_ref(local.obj, gc_state)) {
          scan_base_obj(gc_state, (void*)local.obj->heap_ptr);
        }
      }
    }
  }

  return 0;
}

/*
 * Scan the static fields for all classes that
 * have been loaded by the bootstrap loader.
 */
// WRITE ME
static int scan_class_map(gc_state_t* gc_state, void* priv_data) {
  struct nk_hashtable* classmap = priv_data;
  struct nk_hashtable_iter* iter;  // interator for the hashtable

  // check if the classmap is empty
  if (nk_htable_count(classmap) == 0) return 0;

  // start iterating over the hashmap
  iter = nk_create_htable_iter(classmap);
  while (1) {
    java_class_t* cls = (java_class_t*)nk_htable_get_iter_value(iter);

    for (int i = 0; i < cls->fields_count; i++) {
      field_info_t* fld = &cls->fields[i];

      var_t* val = fld->value;
      // if the field not been allocated, check if it is a reference or not
      if (val != NULL) {
        if (val->tag == TAG_OBJ) {
          if (val->obj->heap_ptr != 0) {
            if (!remark_ref(val->obj, gc_state)) {
              scan_base_obj(gc_state, (native_obj_t*)val->obj->heap_ptr);
            } else {
              printf("already marked!\n");
            }
          }
        }
      }
    }

    if (nk_htable_iter_advance(iter) == 0) break;
  }

  nk_destroy_htable_iter(iter);
  return 0;
}

#define MIL 1000000
#define BIL 1000000000UL
/*
 * Determines whether or not its time to collect garbage.
 * The GC will default to a 20ms timeout.
 *
 * @return: 1 if we should collect, 0 otherwise
 *
 */
int gc_should_collect(jthread_t* t) {
  gc_time_t* time = NULL;
  struct timespec s;
  u8 cur_time_ms;

  // we can't collect if GC state hasn't been properly initialized
  if (!t || !t->gc_state) {
    return 0;
  }

  time = &(t->gc_state->time_info);

  clock_gettime(CLOCK_REALTIME, &s);

  cur_time_ms = (s.tv_nsec / MIL) + (s.tv_sec * 1000);

  if (cur_time_ms - time->last_collected_ms > time->interval_ms) {
    return 1;
  }

  return 0;
}

/*
 * The main interface to the GC. Calling this function will
 * initiate the mark and sweep process.
 *
 * If dump_stats is one, we will also get some verbose output
 * including how much was collected, and how much time it took.
 *
 */
int gc_collect(jthread_t* t) {
  struct timespec s, e;
  gc_stats_t* stats = &t->gc_state->collect_stats;
  u8 runtime_ns;

  memset(stats, 0, sizeof(gc_stats_t));

  clock_gettime(CLOCK_REALTIME, &s);

  if (mark(t->gc_state) != 0) {
    HB_ERR("GC could not mark");
    return -1;
  }

  clock_gettime(CLOCK_REALTIME, &e);

  stats->mark_time_ns = (e.tv_sec - s.tv_sec) * BIL + (e.tv_nsec - s.tv_nsec);
  clock_gettime(CLOCK_REALTIME, &s);

  if (sweep(t->gc_state) != 0) {
    HB_ERR("GC could not sweep");
    return -1;
  }

  clock_gettime(CLOCK_REALTIME, &e);

  stats->sweep_time_ns = (e.tv_sec - s.tv_sec) * BIL + (e.tv_nsec - s.tv_nsec);

  stats->gc_time_ns = stats->mark_time_ns + stats->sweep_time_ns;

  runtime_ns = ((e.tv_sec * BIL) + (e.tv_nsec)) -
               t->gc_state->time_info.gc_start_time_ns;

  if (t->gc_state->trace) {
    HB_INFO("GC STATS (current runtime: %lu.%lums):", runtime_ns / MIL,
            (runtime_ns % MIL));
    HB_INFO("  Objects collected: %d", stats->obj_collected);
    HB_INFO("  Heap Reclaimed:    %dB", stats->bytes_reclaimed);
    HB_INFO("  GC Time:           %lu.%lums", stats->gc_time_ns / MIL,
            stats->gc_time_ns % MIL);
    HB_INFO("  |__Mark:           %lu.%lums", stats->mark_time_ns / MIL,
            stats->mark_time_ns % MIL);
    HB_INFO("  |__Sweep:          %lu.%lums", stats->sweep_time_ns / MIL,
            stats->sweep_time_ns % MIL);
  }

  // reset the timer
  t->gc_state->time_info.last_collected_ms =
      (e.tv_nsec / MIL) + (e.tv_sec * 1000);

  return 0;
}

/*
 * KCH NOTE: this is used for
 * references to objects that were allocated
 * before the GC was initialized
 */
int gc_insert_ref(obj_ref_t* ref) { return ref_tbl_insert_ref(ref); }

/*
 * The base object has already been allocated *outside*
 * of the GC system. We have to keep track of its reference,
 * else it will be collected (it should never be)
 *
 */
int gc_init(jthread_t* main, obj_ref_t* base_obj, bool trace, int interval) {
  native_obj_t* obj = (native_obj_t*)base_obj->heap_ptr;
  struct timespec s;

  main->gc_state = malloc(sizeof(gc_state_t));

  if (!main->gc_state) {
    HB_ERR("Could not create GC state");
    return -1;
  }

  memset(main->gc_state, 0, sizeof(gc_state_t));

  if (init_ref_tbl(main->gc_state) != 0) {
    HB_ERR("Could not init ref table");
    return -1;
  }

  INIT_LIST_HEAD(&main->gc_state->root_list);

  // add the base obj ref to root list
  add_root(base_obj, scan_base_ref, "Base Object Ref.", main->gc_state);
  add_root(obj, scan_base_obj, "Base Object", main->gc_state);
  add_root(main->cur_frame, scan_base_frame, "Base Frame", main->gc_state);
  add_root(hb_get_classmap(), scan_class_map, "Class Map", main->gc_state);

  if (ref_tbl_insert_ref(base_obj) != 0) {
    HB_ERR("Could not insert base object ref into ref table");
    return -1;
  }

  main->gc_state->trace = trace;

  if (interval) {
    main->gc_state->time_info.interval_ms = interval;
  } else {
    main->gc_state->time_info.interval_ms = GC_DEFAULT_INTERVAL;
  }

  clock_gettime(CLOCK_REALTIME, &s);

  u8 stamp = s.tv_nsec + (s.tv_sec * 1000000000);

  main->gc_state->time_info.last_collected_ms = stamp / 1000000;
  main->gc_state->time_info.gc_start_time_ns = stamp;

  GC_DEBUG("GC Initialized.");

  return 0;
}
