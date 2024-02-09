/*
 * Copyright ©2023 Chris Thachuk.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Fall Quarter 2023 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>
#include <stdlib.h>

#include "CSE333.h"
#include "LinkedList.h"
#include "LinkedList_priv.h"


///////////////////////////////////////////////////////////////////////////////
// LinkedList implementation.

LinkedList* LinkedList_Allocate(void) {
  // Allocate the linked list record.
  LinkedList *ll = (LinkedList *) malloc(sizeof(LinkedList));
  Verify333(ll != NULL);

  // STEP 1: initialize the newly allocated record structure.
  ll->num_elements = 0;
  ll->head = NULL;
  ll->tail = NULL;

  // Return our newly minted linked list.
  return ll;
}

void LinkedList_Free(LinkedList *list,
                     LLPayloadFreeFnPtr payload_free_function) {
  Verify333(list != NULL);
  Verify333(payload_free_function != NULL);

  // STEP 2: sweep through the list and free all of the nodes' payloads
  // (using the payload_free_function supplied as an argument) and
  // the nodes themselves.

  // Track the current node, starting with the head of the list.
  LinkedListNode *curr = list->head;

  // Continue to free the current node as long as it is non-null.
  while (curr != NULL) {
    payload_free_function(curr->payload);
    LinkedListNode *next = curr->next;
    free(curr);
    curr = next;
  }

  // Free the whole LinkedList.
  free(list);
}

int LinkedList_NumElements(LinkedList *list) {
  Verify333(list != NULL);
  return list->num_elements;
}

void LinkedList_Push(LinkedList *list, LLPayload_t payload) {
  Verify333(list != NULL);

  // Allocate space for the new node.
  LinkedListNode *ln = (LinkedListNode *) malloc(sizeof(LinkedListNode));
  Verify333(ln != NULL);

  // Set the payload
  ln->payload = payload;

  if (list->num_elements == 0) {
    // Degenerate case; list is currently empty
    Verify333(list->head == NULL);
    Verify333(list->tail == NULL);
    ln->next = ln->prev = NULL;
    list->head = list->tail = ln;
    list->num_elements = 1;
  } else {
    // STEP 3: typical case; list has >=1 elements
    // Ensure that the new node's prev is NULL, next is current head.
    ln->prev = NULL;
    ln->next = list->head;

    // Ensure the head's prev is the new node.
    list->head->prev = ln;

    // The new head is the new node.
    list->head = ln;

    // Increase the number of elements in the list.
    list->num_elements++;
  }
}

bool LinkedList_Pop(LinkedList *list, LLPayload_t *payload_ptr) {
  Verify333(payload_ptr != NULL);
  Verify333(list != NULL);

  // STEP 4: implement LinkedList_Pop.  Make sure you test for
  // and empty list and fail.  If the list is non-empty, there
  // are two cases to consider: (a) a list with a single element in it
  // and (b) the general case of a list with >=2 elements in it.
  // Be sure to call free() to deallocate the memory that was
  // previously allocated by LinkedList_Push().

  // Case 1: an empty list cannot pop anything off.
  if (list->num_elements == 0) {
    return false;
  }

  // Store the payload pointer in the location pointed to by the return
  // parameter.
  LinkedListNode *popped_node_ptr = list->head;
  *payload_ptr = popped_node_ptr->payload;

  if (list->num_elements == 1) {
    // Case 2: a single element list will have no head or tail.
    list->head = NULL;
    list->tail = NULL;
  } else {
    // Case 3: >= 2 elements
    // Remove the new head's backreference and reassign the list's head.
    LinkedListNode *new_head_ptr = popped_node_ptr->next;
    new_head_ptr->prev = NULL;
    list->head = new_head_ptr;
  }

  // Reduce the size of the list.
  list->num_elements--;

  // Free the memory allocated for the popped node.
  free(popped_node_ptr);

  return true;
}

void LinkedList_Append(LinkedList *list, LLPayload_t payload) {
  Verify333(list != NULL);

  // STEP 5: implement LinkedList_Append.  It's kind of like
  // LinkedList_Push, but obviously you need to add to the end
  // instead of the beginning.

  // Allocate space for the new node.
  LinkedListNode *ln = (LinkedListNode *) malloc(sizeof(LinkedListNode));
  Verify333(ln != NULL);

  // Initialize the node.
  ln->next = NULL;
  ln->payload = payload;

  if (list->num_elements == 0) {
    // Case 1: empty list
    ln->prev = NULL;
    list->head = ln;
    list->tail = ln;
  } else {
    // Case 2: general case, >= 1 elements
    ln->prev = list->tail;
    list->tail->next = ln;
    list->tail = ln;
  }

  // Increase the size of the list.
  list->num_elements++;
}

void LinkedList_Sort(LinkedList *list, bool ascending,
                     LLPayloadComparatorFnPtr comparator_function) {
  Verify333(list != NULL);
  if (list->num_elements < 2) {
    // No sorting needed.
    return;
  }

  // We'll implement bubblesort! Nnice and easy, and nice and slow :)
  int swapped;
  do {
    LinkedListNode *curnode;

    swapped = 0;
    curnode = list->head;
    while (curnode->next != NULL) {
      int compare_result = comparator_function(curnode->payload,
                                               curnode->next->payload);
      if (ascending) {
        compare_result *= -1;
      }
      if (compare_result < 0) {
        // Bubble-swap the payloads.
        LLPayload_t tmp;
        tmp = curnode->payload;
        curnode->payload = curnode->next->payload;
        curnode->next->payload = tmp;
        swapped = 1;
      }
      curnode = curnode->next;
    }
  } while (swapped);
}


///////////////////////////////////////////////////////////////////////////////
// LLIterator implementation.

LLIterator* LLIterator_Allocate(LinkedList *list) {
  Verify333(list != NULL);

  // OK, let's manufacture an iterator.
  LLIterator *li = (LLIterator *) malloc(sizeof(LLIterator));
  Verify333(li != NULL);

  // Set up the iterator.
  li->list = list;
  li->node = list->head;

  return li;
}

void LLIterator_Free(LLIterator *iter) {
  Verify333(iter != NULL);
  free(iter);
}

bool LLIterator_IsValid(LLIterator *iter) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);

  return (iter->node != NULL);
}

bool LLIterator_Next(LLIterator *iter) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  // STEP 6: try to advance iterator to the next node and return true if
  // you succeed, false otherwise
  // Note that if the iterator is already at the last node,
  // you should move the iterator past the end of the list

  // Assign the node pointer to the next node.
  iter->node = iter->node->next;

  // The iterator is valid as long as the node pointer is non-null.
  return iter->node != NULL;
}

void LLIterator_Get(LLIterator *iter, LLPayload_t *payload) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  *payload = iter->node->payload;
}

bool LLIterator_Remove(LLIterator *iter,
                       LLPayloadFreeFnPtr payload_free_function) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  // STEP 7: implement LLIterator_Remove.  This is the most
  // complex function you'll build.  There are several cases
  // to consider:
  // - degenerate case: the list becomes empty after deleting.
  // - degenerate case: iter points at head
  // - degenerate case: iter points at tail
  // - fully general case: iter points in the middle of a list,
  //                       and you have to "splice".
  //
  // Be sure to call the payload_free_function to free the payload
  // the iterator is pointing to, and also free any LinkedList
  // data structure element as appropriate.

  // Store the payload pointer in the stack frame.
  LLPayload_t payload = NULL;

  if (iter->node->prev == NULL) {
    // Case 1 & 2: the list has one element or the current node is the head
    // Pop off the top node.
    LinkedList_Pop(iter->list, &payload);

    // The iterator should now be pointing at the head of the list.
    iter->node = iter->list->head;
  } else if (iter->node->next == NULL) {
    // Case 3: the current node is the tail
    // Slice off the last node.
    LLSlice(iter->list, &payload);

    // The iterator should now be pointing at the tail of the list.
    iter->node = iter->list->tail;
  } else {
    // Case 4: the current node is in the middle of the list
    // Remove the node by rearranging the pointers of previous and next nodes.
    LinkedListNode *curr = iter->node;
    curr->prev->next = curr->next;
    curr->next->prev = curr->prev;
    payload = curr->payload;

    // Update the list size.
    iter->list->num_elements--;

    // The iterator should now be pointing at the next node.
    iter->node = curr->next;

    // Free the current node.
    free(curr);
  }

  // Free the payload.
  payload_free_function(payload);

  // The list is still valid as long as it is not empty.
  return iter->list->num_elements != 0;
}


///////////////////////////////////////////////////////////////////////////////
// Helper functions

bool LLSlice(LinkedList *list, LLPayload_t *payload_ptr) {
  Verify333(payload_ptr != NULL);
  Verify333(list != NULL);

  // STEP 8: implement LLSlice.
  // Case 1: the list is empty, and slice fails.
  if (list->num_elements == 0) {
    return false;
  }

  // Get a pointer to the sliced node.
  LinkedListNode *sliced_node_ptr = list->tail;

  // Store the payload of the sliced node in the location pointed to by the
  // return param.
  *payload_ptr = sliced_node_ptr->payload;

  if (list->num_elements == 1) {
    // Case 2: the list has only one element
    list->head = NULL;
    list->tail = NULL;
  } else {
    // Case 3: the list has >= 2 elements
    // Remove the forward reference of the new tail, and reassign tail.
    LinkedListNode *new_tail_ptr = list->tail->prev;
    new_tail_ptr->next = NULL;
    list->tail = new_tail_ptr;
  }

  // Update the size of the linked list.
  list->num_elements--;

  // Free the sliced node.
  free(sliced_node_ptr);

  return true;
}

void LLIteratorRewind(LLIterator *iter) {
  iter->node = iter->list->head;
}
