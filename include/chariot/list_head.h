#pragma once

#define LIST_POISON1 ((void *)0x00100100)
#define LIST_POISON2 ((void *)0x00200200)


#define LIST_HEAD_INIT(name) { &(name), &(name) }

/*
 * a list_head is a concept I blatently stole from the linux kernel. These are meant
 * to exist within a structure so you can point around
 */
struct list_head {
  struct list_head *prev = this;
  struct list_head *next = this;


  inline void init(void) {
    prev = next = this;
  }

  /*
   * Insert a new entry between two known consecutive entries.
   *
   * This is only for internal list manipulation where we know
   * the prev/next entries already!
   */
  static inline void __add(struct list_head *n, struct list_head *prev, struct list_head *next) {
    next->prev = n;
    n->next = next;
    n->prev = prev;
    prev->next = n;
  }


  /**
   * add - add a new entry
   * @n: new entry to be added
   *
   * Insert a new entry after the specified head.
   * This is good for implementing stacks.
   */
  inline void add(struct list_head *n) {
    __add(n, this, this->next);
  }

  /**
   * add_tail - add a new entry
   * @n: new entry to be added
   * @head: list head to add it before
   *
   * Insert a new entry before the specified head.
   * This is useful for implementing queues.
   */
  inline void add_tail(struct list_head *n) {
    __add(n, this->prev, this);
  }



  /*
   * Delete a list entry by making the prev/next entries
   * point to each other.
   *
   * This is only for internal list manipulation where we know
   * the prev/next entries already!
   */
  static inline void __del(struct list_head *prev, struct list_head *next) {
    next->prev = prev;
    prev->next = next;
  }

  /**
   * del - deletes this from list.
   * @this: the element to delete from the list.
   * Note: list_empty() on entry does not return true after this, the entry is
   * in an undefined state.
   */
  inline void del(void) {
    __del(this->prev, this->next);
    this->next = (struct list_head *)LIST_POISON1;
    this->prev = (struct list_head *)LIST_POISON2;
  }

  /**
   * list_del_init - deletes entry from list and reinitialize it.
   * @this: the element to delete from the list.
   */
  inline void del_init(void) {
    del();
    init();
  }


  inline void replace(struct list_head *other) {
    other->next = this->next;
    other->next->prev = other;
    other->prev = this->prev;
    other->prev->next = other;
  }


  inline void replace_init(struct list_head *other) {
    replace(other);
    init();
  }


  /**
   * list_move - delete from one list and add as another's head
   * @head: the head that will precede our entry
   */
  inline void move(struct list_head *head) {
    del();            // remove from the current list
    head->add(this);  // add after the next element
  }


  /**
   * list_move_tail - delete from one list and add as another's tail
   * @head: the head that will follow our entry
   */
  inline void list_move_tail(struct list_head *head) {
    this->del();
    head->add_tail(this);
  }


  /**
   * is_last - tests whether @entry is the last entry in list @this
   * @entry: the entry to test
   */
  inline bool is_last(struct list_head *entry) {
    return entry->next == this;
  }


  /**
   * is_empty - test whether the list is empty
   */
  inline bool is_empty() {
    return prev == next;
  }
  /**
   * list_empty_careful - tests whether a list is empty and not being modified
   *
   * Description:
   * tests whether a list is empty _and_ checks that no other CPU might be
   * in the process of modifying either member (next or prev)
   *
   * NOTE: using is_empty_careful() without synchronization
   * can only be safe if the only activity that can happen
   * to the list entry is list_del_init(). Eg. it cannot be used
   * if another CPU could re-list_add() it.
   */
  inline bool is_empty_careful() {
    struct list_head *onext = this->next;
    return (onext == this) && (onext == this->prev);
  }



  static inline void __splice(const struct list_head *list, struct list_head *prev,
                              struct list_head *next) {
    struct list_head *first = list->next;
    struct list_head *last = list->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
  }


  inline void splice(struct list_head *other) {
    if (!other->is_empty()) __splice(other, this, this->next);
  }



  /**
   * list_splice_tail - join two lists, each list being a queue
   * @list: the new list to add.
   * @head: the place to add it in the first list.
   */
  inline void splice_tail(struct list_head *list) {
    if (!list->is_empty()) __splice(list, this->prev, this);
  }



  inline void splice_init(struct list_head *other) {
    if (!other->is_empty()) {
      __splice(other, this, this->next);
      other->init();
    }
  }
  inline void splice_tail_init(struct list_head *other) {
    if (!other->is_empty()) {
      __splice(other, this->prev, this);
      other->init();
    }
  }




  struct iter {
    struct list_head *item;

    struct list_head::iter &operator++() {
      item = item->next;
      return *this;
    }

    struct list_head::iter &operator--() {
      item = item->prev;
      return *this;
    }

    struct list_head *operator*(void) {
      return item;
    }


    bool operator==(const iter &other) const {
      return item == other.item;
    }
    bool operator!=(const iter &other) const {
      return item != other.item;
    }
  };


  struct list_head::iter begin() {
    return list_head::iter{next};
  }
  struct list_head::iter end() {
    return list_head::iter{this};
  }
};



template <class T>
struct __remove_reference {
  typedef T type;
};
template <class T>
struct __remove_reference<T &> {
  typedef T type;
};
template <class T>
struct __remove_reference<T &&> {
  typedef T type;
};

#define __decltype(t) typename __remove_reference<decltype(t)>::type

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member)                      \
  ({                                                       \
    const __decltype(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member));     \
  })

/**
 * list_first_entry - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

/**
 * list_next_entry - get the next element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, __decltype(*(pos)), member)

/**
 * list_entry_is_head - test if the entry points to the head of the list
 * @pos:	the type * to cursor
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry_is_head(pos, head, member)				\
	(&pos->member == (head))


/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_first_entry(head, __decltype(*pos), member);	\
	     !list_entry_is_head(pos, head, member);			\
	     pos = list_next_entry(pos, member))


/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)            \
  for (pos = list_entry((head)->next, __decltype(*pos), member),  \
      n = list_entry(pos->member.next, __decltype(*pos), member); \
       &pos->member != (head); pos = n, n = list_entry(n->member.next, __decltype(*n), member))
