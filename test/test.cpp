#include "table.h"
#include "vm.h"
#include "object.h"
#include <stdio.h>

int main() {
    VM vm;
    Table table;

    char str[4];
    str[0] = 'f';
    str[1] = '0';
    str[2] = '0';
    str[3] = '\0';

    for (int i = 0; i < 100; i++) {
        str[1] = '0' + (i / 10);
        str[2] = '0' + (i % 10);
        Value s = string_value(&vm, str, 3);
        Value v = NUMBER_VAL(0.0 + i);
        table.insert(AS_STRING(s), v);
    }

    print_table(&table);
    printf("table  capacity: %d\tcount: %d\twith_tombstones: %d\n\n", table.get_capacity(), table.get_count(), table.get_count_with_tombstones());

    for (int i = 0; i < 50; i++) {
        str[1] = '0' + (i / 10);
        str[2] = '0' + (i % 10);
        Value s = string_value(&vm, str, 3);
        table.remove(AS_STRING(s));
    }

    print_table(&table);
    printf("table  capacity: %d\tcount: %d\twith_tombstones: %d\n\n", table.get_capacity(), table.get_count(), table.get_count_with_tombstones());

    for (int i = 0; i < 100; i += 2) {
        str[1] = '0' + (i / 10);
        str[2] = '0' + (i % 10);
        Value s = string_value(&vm, str, 3);
        Value v = NUMBER_VAL(0.0 + i);
        table.insert(AS_STRING(s), v);
    }

    print_table(&table);
    printf("table  capacity: %d\tcount: %d\twith_tombstones: %d\n\n", table.get_capacity(), table.get_count(), table.get_count_with_tombstones());

    Table table2;
    table2.insert_all(&table);

    print_table(&table2);
    printf("table2 capacity: %d\tcount: %d\twith_tombstones: %d\n\n", table2.get_capacity(), table2.get_count(), table2.get_count_with_tombstones());

    print_table(&table);
    printf("table  capacity: %d\tcount: %d\twith_tombstones: %d\n\n", table.get_capacity(), table.get_count(), table.get_count_with_tombstones());

    for (int i = 0; i < 100; i++) {
        str[1] = '0' + (i / 10);
        str[2] = '0' + (i % 10);
        Value s = string_value(&vm, str, 3);
        table.remove(AS_STRING(s));
        table2.remove(AS_STRING(s));
    }

    print_table(&table);
    printf("table  capacity: %d\tcount: %d\twith_tombstones: %d\n\n", table.get_capacity(), table.get_count(), table.get_count_with_tombstones());

    print_table(&table2);
    printf("table2 capacity: %d\tcount: %d\twith_tombstones: %d\n\n", table2.get_capacity(), table2.get_count(), table2.get_count_with_tombstones());

    return 0;
}

