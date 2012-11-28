
#include <string.h>
#include <stdlib.h>

#include <vlib/test.h>
#include <vlib/hashtable.h>

static void hinit(Hashtable* h) {
  hashtable_init7(h, hasher_fnv64str, equaler_str, sizeof(char*), sizeof(int), 1, 0.5);
}
static void hclose(Hashtable* h) {
  int free_key(void* _key, void* val) {
    char** key = _key;
    free(*key);
    return HT_CONTINUE;
  }
  hashtable_iter(h, free_key);
  hashtable_close(h);
}
static void hinsert(Hashtable* h, const char* _key, int val) {
  char* key = strdup(_key);
  int* ptr = hashtable_insert(h, &key);
  *ptr = val;
}
static int hget(Hashtable* h, const char* key) {
  return *(int*)hashtable_get(h, &key);
}
static void hupdate(Hashtable* h, const char* key, int val) {
  *(int*)hashtable_get(h, &key) = val;
}
static void hremove(Hashtable* h, const char* key) {
  char* oldkey;
  hashtable_remove(h, &key, &oldkey);
  free(oldkey);
}

static int hashtable_basic() {
  Hashtable h;
  hinit(&h);

  hinsert(&h, "one", 10);
  hinsert(&h, "two", 20);
  hinsert(&h, "trhee", 3);
  hinsert(&h, "four", 4);
  hinsert(&h, "five", 5);

  hupdate(&h, "one", 1);
  hupdate(&h, "two", 2);
  hremove(&h, "trhee");
  hinsert(&h, "three", 3);

  assertEqual(hget(&h, "five"), 5);
  assertEqual(hget(&h, "four"), 4);
  assertEqual(hget(&h, "three"), 3);
  assertEqual(hget(&h, "two"), 2);
  assertEqual(hget(&h, "one"), 1);

  const char* k = "trhee";
  assertEqual(hashtable_get(&h, &k), NULL);

  hclose(&h);
  return 0;
}

VLIB_SUITE(hashtable) = {
  VLIB_TEST(hashtable_basic),
  VLIB_END,
};
