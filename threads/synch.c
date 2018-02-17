/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      thread_try_donate_priority();
      list_push_back(&sema->waiters, &thread_current()->elem);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) {
    sema->value--;
    success = true; 
  }
  else {
	  thread_try_donate_priority();
	  success = false;
  }	  
  
  intr_set_level (old_level);

  return success;
}

/* 
   Modified By: William Van Cleve, Shawn Kirby and Connor McElroy
   
   Changes Inspired By: https://github.com/yuan901202/pintos_2
       https://github.com/ryantimwilson/Pintos-Project-1
	   https://github.com/microdog/pintos-project-1
	   https://github.com/nekketsuing/Pintos-Project-1
   
   Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;
  struct thread *highest_priority_thread = NULL;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) {
	struct list_elem *highest_priority_waiter = list_max(&sema->waiters, priority_less_func, NULL); // Get highest priority waiter
	list_remove(highest_priority_waiter); // Remove highest priority waiter
	highest_priority_thread = list_entry(highest_priority_waiter, struct thread, elem); // Get highest priority waiter's thread
	thread_unblock(highest_priority_thread); // Unblock highest priority thread
  }
  
  sema->value++;
  intr_set_level (old_level);
  if (highest_priority_thread != NULL && thread_current()->priority < highest_priority_thread->priority) thread_yield(); // Yield to highest priority
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/* 
   Modified By: William Van Cleve, Shawn Kirby and Connor McElroy
   
   Changes Inspired By: https://github.com/yuan901202/pintos_2
       https://github.com/ryantimwilson/Pintos-Project-1
	   https://github.com/microdog/pintos-project-1
	   https://github.com/nekketsuing/Pintos-Project-1

   Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  enum intr_level old_level = intr_disable();
  
  struct thread *current = thread_current(); // Get current thread
  if (lock->holder != NULL) { // Lock is current being held
	  current->waiting_lock = lock; // Set waiting lock
	  list_insert_ordered(&lock->holder->donations, &current->donation_elem, priority_less_func, NULL); // Update donations list
  }
  
  sema_down (&lock->semaphore);
  current->waiting_lock = NULL; // Clear waiting lock because it has been received
  lock->holder = current;
  intr_set_level(old_level);
}

/* 
   Modified By: William Van Cleve, Shawn Kirby and Connor McElroy
   
   Changes Inspired By: https://github.com/yuan901202/pintos_2
       https://github.com/ryantimwilson/Pintos-Project-1
	   https://github.com/microdog/pintos-project-1
	   https://github.com/nekketsuing/Pintos-Project-1

   Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  enum intr_level old_level = intr_disable();
  
  success = sema_try_down (&lock->semaphore);
  if (success) { // If lock acquired
	  struct thread *current = thread_current(); // Get current thread
	  current->waiting_lock = NULL; // Clear waiting lock
	  lock->holder = current; // Set lock's holder to current thread
  }
    
  intr_set_level(old_level);
  return success;
}

/* 
   Modified By: William Van Cleve, Shawn Kirby and Connor McElroy
   
   Changes Inspired By: https://github.com/yuan901202/pintos_2
       https://github.com/ryantimwilson/Pintos-Project-1
	   https://github.com/microdog/pintos-project-1
	   https://github.com/nekketsuing/Pintos-Project-1

   Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  bool yield = false; // Indicates whether the current thread needs to yield or not	
	
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));
  
  enum intr_level old_level = intr_disable();
    
  lock->holder = NULL; // Reset lock holder
  struct thread *current = thread_current(); // Get current thread
  
  struct list_elem *e = list_begin(&current->donations); // Get first element in donations list
  while (e != list_end(&current->donations)) {
	struct thread *t = list_entry(e, struct thread, donation_elem); // Get element's thread
	if(t->waiting_lock == lock) list_remove(e); // If the element's thread is waiting on the lock to be released remove it from donation list
	e = list_next(e); // Get next element in list
  }

  if (current->original_priority < current->priority) yield = true;
  current->priority = current->original_priority; // Reset current thread's priority to its original priority	
  
  if (!list_empty (&current->donations)) {
      struct thread *highest_priority_donor = list_entry(list_front(&current->donations), struct thread, donation_elem); // Get highest priority donor
	  if (current->original_priority < highest_priority_donor->priority) current->priority = highest_priority_donor->priority; // If donor needs updated priority update it
  }
  
  sema_up (&lock->semaphore);
  intr_set_level(old_level);
  if (yield) thread_yield(); // If current thread needs to yield then yield
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  list_push_back (&cond->waiters, &waiter.elem);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}


/* 
   Added By: William Van Cleve, Shawn Kirby and Connor McElroy
   
   Inspired By: https://github.com/yuan901202/pintos_2
       https://github.com/ryantimwilson/Pintos-Project-1
	   https://github.com/microdog/pintos-project-1
	   https://github.com/nekketsuing/Pintos-Project-1
	   
	Compares priority of semaphores                                                                       */
static bool sem_less_priority_func(const struct list_elem *e1, const struct list_elem *e2, void *aux UNUSED)
{
  struct semaphore_elem *s1 = list_entry(e1, struct semaphore_elem, elem); // Get semaphore element 1
  struct semaphore_elem *s2 = list_entry(e2, struct semaphore_elem, elem); // Get semaphore element 2
  struct list_elem *max_s1 = list_max(&s1->semaphore.waiters, priority_less_func, NULL); // Get max element in semaphore 1's waiting list
  struct list_elem *max_s2 = list_max(&s2->semaphore.waiters, priority_less_func, NULL); // Get max element in semaphore 2's waiting list
  struct thread *t1 = list_entry(max_s1, struct thread, elem); // Get semaphore 1's highest priority thread
  struct thread *t2 = list_entry(max_s2, struct thread, elem); // Get semaphore 2's highest priority thread
  return t1->priority < t2->priority; // Return whether thread 1 or thread 2 has a higher priority
}

/* 
   Modified By: William Van Cleve, Shawn Kirby and Connor McElroy
   
   Changes Inspired By: https://github.com/yuan901202/pintos_2
       https://github.com/ryantimwilson/Pintos-Project-1
	   https://github.com/microdog/pintos-project-1
	   https://github.com/nekketsuing/Pintos-Project-1

   If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler.                                       */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
	  struct list_elem *max_elem = list_max(&cond->waiters, sem_less_priority_func, NULL); // Get max element
	  list_remove(max_elem); // Remove max element
	  sema_up(&list_entry(max_elem, struct semaphore_elem, elem)->semaphore);	  
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}