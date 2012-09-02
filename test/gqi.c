
#include <assert.h>
#include <stdlib.h>

#include <vlib/test.h>
#include <vlib/gqi.h>
#include <vlib/ini.h>

static void add_person(GQI* mux, const char* id, const char* name) {
  GQI* val = gqic_new_value(name);
  gqic_mux_register(mux, id, val);
  gqi_release(val);
}
static GQI* setup_peopledb() {
  GQI* db = gqi_new_mux();
  add_person(db, "1", "Vaughan Newton");
  add_person(db, "2", "Seth Newton");
  add_person(db, "3", "Bob Marley");
  add_person(db, "4", "Joe Fish");
  return db;
}

static GQI* setup_configdb() {
  const char* src =
    "[wmd]\n"
    "password=1234\n"
    "kilotons=18\n"
    "type=Nuclear Warhead\n"
    "[bunnies]\n"
    "kind=fluffy\n"
    "current_pop=55\n"
    "next_pop=89\n";

  INI* ini = malloc(sizeof(INI));
  ini_init(ini);
  const char* err;
  int r = ini_parse(ini, src, &err);
  assert(r == 0);

  return gqi_new_ini(ini, 1);
}

static int check(GQI* db, const char* query, const char* expect) {
  char* result;
  int r = gqic_query(db, query, &result);
  assert(r == 0);
  int cmp = strcmp(result, expect);
  free(result);
  return cmp == 0;
}

static int gqi_basic() {
  GQI* db = gqi_new_mux();

  GQI* people = setup_peopledb();
  gqic_mux_register(db, "person/", people);
  gqi_release(people);

  GQI* config = setup_configdb();
  gqic_mux_register(db, "config/", config);
  gqi_release(config);

  assertTrue(check(db, "person/3", "Bob Marley"));
  assertTrue(check(db, "person/4", "Joe Fish"));
  assertTrue(check(db, "config/bunnies/kind", "fluffy"));
  assertTrue(check(db, "config/wmd/type", "Nuclear Warhead"));
  assertTrue(check(db, "config/wmd/password", "1234"));

  gqi_release(db);
  return 0;
}

VLIB_SUITE(gqi) = {
  VLIB_TEST(gqi_basic),
  VLIB_END
};
