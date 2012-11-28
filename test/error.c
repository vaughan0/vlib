
#include <vlib/test.h>
#include <vlib/error.h>

static int error_catch() {
  error_t caught;
  TRY {
    verr_raise(1234);
    caught = 0;
  } CATCH(err) {
    caught = err;
  } ETRY
  assertEqual(caught, 1234);
  return 0;
}

VLIB_SUITE(error) = {
  VLIB_TEST(error_catch),
  VLIB_END,
};
