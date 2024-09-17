#include "globals.h"
#include "object.h"
#include <time.h>

static Value clock_native(int argc, Value* args) {
    return NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
}

void define_globals(VM* vm) {
    define_native(vm, "clock", clock_native);
}
