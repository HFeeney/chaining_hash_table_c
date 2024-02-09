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
#include <stdint.h>

#include "CSE333.h"
#include "HashTable.h"
#include "LinkedList.h"
#include "HashTable_priv.h"

///////////////////////////////////////////////////////////////////////////////
// Internal helper functions.
//
#define INVALID_IDX -1

// Grows the hashtable (ie, increase the number of buckets) if its load
// factor has become too high.
static void MaybeResize(HashTable *ht);

// Finds the node within the provided list that contains the key k. The list
// must be non-null. Returns an iterator whose node field points at the node
// containing the key, or at NULL if the key was not found. It is up to the
// client to free the iterator.
static LLIterator* LinkedList_FindKey(LinkedList *list, HTKey_t k);

int HashKeyToBucketNum(HashTable *ht, HTKey_t key) {
  return key % ht->num_buckets;
}

// Frees the payload associated with a linked list node.
static void HTKeyValuePtrFree(LLPayload_t payload) {
  free(payload);
}

// Deallocation functions that do nothing.  Useful if we want to deallocate
// the structure (eg, the linked list) without deallocating its elements or
// if we know that the structure is empty.
static void LLNoOpFree(LLPayload_t freeme) { }
static void HTNoOpFree(HTValue_t freeme) { }


///////////////////////////////////////////////////////////////////////////////
// HashTable implementation.

HTKey_t FNVHash64(unsigned char *buffer, int len) {
  // This code is adapted from code by Landon Curt Noll
  // and Bonelli Nicola:
  //     http://code.google.com/p/nicola-bonelli-repo/
  static const uint64_t FNV1_64_INIT = 0xcbf29ce484222325ULL;
  static const uint64_t FNV_64_PRIME = 0x100000001b3ULL;
  unsigned char *bp = (unsigned char *) buffer;
  unsigned char *be = bp + len;
  uint64_t hval = FNV1_64_INIT;

  // FNV-1a hash each octet of the buffer.
  while (bp < be) {
    // XOR the bottom with the current octet.
    hval ^= (uint64_t) * bp++;
    // Multiply by the 64 bit FNV magic prime mod 2^64.
    hval *= FNV_64_PRIME;
  }
  return hval;
}

HashTable* HashTable_Allocate(int num_buckets) {
  HashTable *ht;
  int i;

  Verify333(num_buckets > 0);

  // Allocate the hash table record.
  ht = (HashTable *) malloc(sizeof(HashTable));
  Verify333(ht != NULL);

  // Initialize the record.
  ht->num_buckets = num_buckets;
  ht->num_elements = 0;
  ht->buckets = (LinkedList **) malloc(num_buckets * sizeof(LinkedList *));
  Verify333(ht->buckets != NULL);
  for (i = 0; i < num_buckets; i++) {
    ht->buckets[i] = LinkedList_Allocate();
  }

  return ht;
}

void HashTable_Free(HashTable *table,
                    ValueFreeFnPtr value_free_function) {
  int i;

  Verify333(table != NULL);

  // Free each bucket's chain.
  for (i = 0; i < table->num_buckets; i++) {
    LinkedList *bucket = table->buckets[i];
    HTKeyValue_t *kv;

    // Pop elements off the chain list one at a time.  We can't do a single
    // call to LinkedList_Free since we need to use the passed-in
    // value_free_function -- which takes a HTValue_t, not an LLPayload_t -- to
    // free the caller's memory.
    while (LinkedList_NumElements(bucket) > 0) {
      Verify333(LinkedList_Pop(bucket, (LLPayload_t *)&kv));
      value_free_function(kv->value);
      free(kv);
    }
    // The chain is empty, so we can pass in the
    // null free function to LinkedList_Free.
    LinkedList_Free(bucket, LLNoOpFree);
  }

  // Free the bucket array within the table, then free the table record itself.
  free(table->buckets);
  free(table);
}

int HashTable_NumElements(HashTable *table) {
  Verify333(table != NULL);
  return table->num_elements;
}

bool HashTable_Insert(HashTable *table,
                      HTKeyValue_t newkeyvalue,
                      HTKeyValue_t *oldkeyvalue) {
  int bucket;
  LinkedList *chain;

  Verify333(table != NULL);
  MaybeResize(table);

  // Calculate which bucket and chain we're inserting into.
  bucket = HashKeyToBucketNum(table, newkeyvalue.key);
  chain = table->buckets[bucket];

  // STEP 1: finish the implementation of InsertHashTable.
  // This is a fairly complex task, so you might decide you want
  // to define/implement a helper function that helps you find
  // and optionally remove a key within a chain, rather than putting
  // all that logic inside here.  You might also find that your helper
  // can be reused in steps 2 and 3.

  // Get an iterator that points to the node containing the matching key.
  LLIterator *lli = LinkedList_FindKey(chain, newkeyvalue.key);

  // If the iterator looped through completely without finding key,
  // add the new key value pair to the end of the current chain.
  if (!LLIterator_IsValid(lli)) {
    // Allocate space for the payload (a key value pointer).
    HTKeyValue_t *kv_ptr = (HTKeyValue_t*) malloc(sizeof(HTKeyValue_t));
    Verify333(kv_ptr != NULL);

    // Initialize the key value pair.
    *kv_ptr = newkeyvalue;

    // Append the key value pair.
    LinkedList_Append(chain, kv_ptr);

    // Free the iterator.
    LLIterator_Free(lli);

    // Update the table's size.
    table->num_elements++;

    // Indicate that this was a new key.
    return false;
  }

  // Otherwise, the iterator now points at the node with the matching key.
  // Store the key value pair pointer in curr (the node payload).
  HTKeyValue_t *curr;
  LLIterator_Get(lli, (LLPayload_t*) &curr);

  // Use the return param to store the key value pair that will be replaced,
  // then replace the pair with the new pair.
  *oldkeyvalue = *curr;
  *curr = newkeyvalue;

  // Free the iterator.
  LLIterator_Free(lli);

  // A pair was replaced.
  return true;
}

bool HashTable_Find(HashTable *table,
                    HTKey_t key,
                    HTKeyValue_t *keyvalue) {
  Verify333(table != NULL);

  int bucket;
  LinkedList *chain;

  // Calculate which bucket and chain we're inserting into.
  bucket = HashKeyToBucketNum(table, key);
  chain = table->buckets[bucket];

  // STEP 2: implement HashTable_Find.
  // Get an iterator stopped at the correct key.
  LLIterator *lli = LinkedList_FindKey(chain, key);

  // If the iterator is not valid, then the key was not found.
  if (!LLIterator_IsValid(lli)) {
    // Free the iterator.
    LLIterator_Free(lli);
    return false;
  }

  // Store the key value pair pointer in curr, then use it to return the
  // pair through the return parameter.
  HTKeyValue_t *curr;
  LLIterator_Get(lli, (LLPayload_t*) &curr);
  *keyvalue = *curr;

  // Free the iterator.
  LLIterator_Free(lli);

  // Indicate that the pair was found.
  return true;
}

bool HashTable_Remove(HashTable *table,
                      HTKey_t key,
                      HTKeyValue_t *keyvalue) {
  Verify333(table != NULL);

  // STEP 3: implement HashTable_Remove.
  int bucket;
  LinkedList *chain;

  // Calculate which bucket and chain we're inserting into.
  bucket = HashKeyToBucketNum(table, key);
  chain = table->buckets[bucket];

  // Get an iterator that points to the node containing the matching key.
  LLIterator *lli = LinkedList_FindKey(chain, key);

  // If the iterator is not valid, then the key was not found.
  if (!LLIterator_IsValid(lli)) {
    // Free the iterator.
    LLIterator_Free(lli);
    return false;
  }

  // Return the pair through the return parameter.
  HTKeyValue_t *curr;
  LLIterator_Get(lli, (LLPayload_t*) &curr);
  *keyvalue = *curr;

  // Remove the current node from the chain.
  LLIterator_Remove(lli, HTKeyValuePtrFree);

  // Free the iterator.
  LLIterator_Free(lli);

  // Update the hash table size.
  table->num_elements--;

  return true;
}


///////////////////////////////////////////////////////////////////////////////
// HTIterator implementation.

HTIterator* HTIterator_Allocate(HashTable *table) {
  HTIterator *iter;
  int         i;

  Verify333(table != NULL);

  iter = (HTIterator *) malloc(sizeof(HTIterator));
  Verify333(iter != NULL);

  // If the hash table is empty, the iterator is immediately invalid,
  // since it can't point to anything.
  if (table->num_elements == 0) {
    iter->ht = table;
    iter->bucket_it = NULL;
    iter->bucket_idx = INVALID_IDX;
    return iter;
  }

  // Initialize the iterator.  There is at least one element in the
  // table, so find the first element and point the iterator at it.
  iter->ht = table;
  for (i = 0; i < table->num_buckets; i++) {
    if (LinkedList_NumElements(table->buckets[i]) > 0) {
      iter->bucket_idx = i;
      break;
    }
  }
  Verify333(i < table->num_buckets);  // make sure we found it.
  iter->bucket_it = LLIterator_Allocate(table->buckets[iter->bucket_idx]);
  return iter;
}

void HTIterator_Free(HTIterator *iter) {
  Verify333(iter != NULL);
  if (iter->bucket_it != NULL) {
    LLIterator_Free(iter->bucket_it);
    iter->bucket_it = NULL;
  }
  free(iter);
}

bool HTIterator_IsValid(HTIterator *iter) {
  Verify333(iter != NULL);
  return iter->bucket_idx != INVALID_IDX;
}

bool HTIterator_Next(HTIterator *iter) {
  Verify333(iter != NULL);

  // STEP 5: implement HTIterator_Next.

  // Ensure that the iterator is valid.
  if (!HTIterator_IsValid(iter)) {
    return false;
  }

  // Get the current list iterator, and attempt to advance it.
  if (LLIterator_Next(iter->bucket_it)) {
    return true;
  }

  // The iterator must have moved past the end of the current bucket's list.
  // At this point a new bucket with valid content has to be found by
  // looping through the buckets array, starting with the next bucket.
  while (++(iter->bucket_idx) < iter->ht->num_buckets) {
    LinkedList *currList = iter->ht->buckets[iter->bucket_idx];
    if (LinkedList_NumElements(currList) > 0) {
      // A valid bucket was found. Update the iterator.
      iter->bucket_it = LLIterator_Allocate(currList);
      return true;
    }
  }

  // Indicate there were no elements after the bucket we advanced past,
  // and mark the iterator as invalid.
  iter->bucket_idx = INVALID_IDX;
  iter->bucket_it = NULL;
  return false;
}

bool HTIterator_Get(HTIterator *iter, HTKeyValue_t *keyvalue) {
  Verify333(iter != NULL);

  // STEP 6: implement HTIterator_Get.

  // Ensure that the iterator is valid (that the list is not empty).
  if (!HTIterator_IsValid(iter)) {
    return false;
  }

  // Use the current iterator's linked list iterator to get the key value pair.
  HTKeyValue_t *kv_ptr;
  LLIterator_Get(iter->bucket_it, (LLPayload_t*) &kv_ptr);
  *keyvalue = *kv_ptr;
  return true;
}

bool HTIterator_Remove(HTIterator *iter, HTKeyValue_t *keyvalue) {
  HTKeyValue_t kv;

  Verify333(iter != NULL);

  // Try to get what the iterator is pointing to.
  if (!HTIterator_Get(iter, &kv)) {
    return false;
  }

  // Advance the iterator.  Thanks to the above call to
  // HTIterator_Get, we know that this iterator is valid (though it
  // may not be valid after this call to HTIterator_Next).
  HTIterator_Next(iter);

  // Lastly, remove the element.  Again, we know this call will succeed
  // due to the successful HTIterator_Get above.
  Verify333(HashTable_Remove(iter->ht, kv.key, keyvalue));
  Verify333(kv.key == keyvalue->key);
  Verify333(kv.value == keyvalue->value);

  return true;
}

static void MaybeResize(HashTable *ht) {
  HashTable *newht;
  HashTable tmp;
  HTIterator *it;

  // Resize if the load factor is > 3.
  if (ht->num_elements < 3 * ht->num_buckets)
    return;

  // This is the resize case.  Allocate a new hashtable,
  // iterate over the old hashtable, do the surgery on
  // the old hashtable record and free up the new hashtable
  // record.
  newht = HashTable_Allocate(ht->num_buckets * 9);

  // Loop through the old ht copying its elements over into the new one.
  for (it = HTIterator_Allocate(ht);
       HTIterator_IsValid(it);
       HTIterator_Next(it)) {
    HTKeyValue_t item, unused;

    Verify333(HTIterator_Get(it, &item));
    HashTable_Insert(newht, item, &unused);
  }

  // Swap the new table onto the old, then free the old table (tricky!).  We
  // use the "no-op free" because we don't actually want to free the elements;
  // they're owned by the new table.
  tmp = *ht;
  *ht = *newht;
  *newht = tmp;

  // Done!  Clean up our iterator and temporary table.
  HTIterator_Free(it);
  HashTable_Free(newht, &HTNoOpFree);
}

static LLIterator* LinkedList_FindKey(LinkedList *list, HTKey_t k) {
  // Allocate the iterator pointing at the first node.
  LLIterator *lli = LLIterator_Allocate(list);

  // If the list is empty, return without attempting to find the key.
  if (!LLIterator_IsValid(lli)) {
    return lli;
  }

  // The payload of each node is a pointer to a key value pair.
  // Use get to replace curr with current node's payload.
  HTKeyValue_t *curr;
  LLIterator_Get(lli, (LLPayload_t*) &curr);

  // Continue to loop until the current node's key is the desired key,
  // or the iterator has fully iterated through the list.
  while (curr->key != k && LLIterator_Next(lli)) {
    LLIterator_Get(lli, (LLPayload_t*) &curr);
  }

  // The iterator is now past the end, or points at the node with the
  // correct key.
  return lli;
}
