#include "hashset.h"

#include <stdio.h>

static const char *test_strings[] =
{
    "The",
    "quick",
    "brown",
    "fox",
    "jumps",
    "over",
    "the",
    "lazy",
    "dog",
};
static const char *str_add_prev = "*AddPrev*";
static const char *str_add_next = "*AddNext*";
static const int test_strings_count = sizeof(test_strings) / sizeof(test_strings[0]);

static void print_str_hash_set(struct hashset *hashset)
{
    struct hashset_iterator iterator = { };
    const char *str = NULL;

    hashset_iterator_init(&iterator, hashset);

    while ((str = hashset_iterator_next(&iterator)))
        printf("%s ", str);
    
    printf("| count: %lu\n", hashset->count);
}

void test_hashset(void)
{
    struct hashset hashset = { };
    struct hashset_iterator iter = { };
    const char *str = NULL;

    puts("- test_hashset -");

    hashset_init(&hashset);

    // add
    for (int i = 0; i < test_strings_count; i++)
        hashset_insert(&hashset, (void *)test_strings[i], NULL);

    print_str_hash_set(&hashset);

    // resize
    hashset_resize(&hashset, 8);
    print_str_hash_set(&hashset);

    // find
    for (int i = 0; i < test_strings_count; i++)
        printf("found: %s\n", (const char *)hashset_find(&hashset, (void *)test_strings[i]));

    // deletion
    for (int i = test_strings_count - 1; i >= test_strings_count / 2; i--)
        hashset_delete(&hashset, (void *)test_strings[i]);

    print_str_hash_set(&hashset);
    
    // cleanup
    hashset_iterator_init(&iter, &hashset);
    while ((str = hashset_iterator_next_deletion(&iter)))
        printf("iter_del: %s\n", str);

    hashset_cleanup(&hashset);
}
