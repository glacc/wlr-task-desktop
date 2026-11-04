#include "ContainerTest.h"

#include "LinkedList.h"
#include "HashSet.h"

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

static void PrintStrLinkedList(struct LinkedList *list)
{
    struct LinkedList_Iter iter = { };
    struct LinkedList_Node *node = NULL;

    LinkedList_IterInit(&iter, list);
    while ((node = LinkedList_IterNext(&iter)))
        printf("%s ", (const char *)node->elem);
    
    printf("| count: %lu\n", list->count);
}

static void PrintStrLinkedListNode(struct LinkedList_Node *node)
{
    if (node)
        puts((const char *)node->elem);
    else
        puts("not found");
}

static void TestLinkedList(void)
{
    struct LinkedList list = { };
    struct LinkedList_Iter iter = { };
    struct LinkedList_Node *node = NULL;

    puts("- TestLinkedList -");

    // insertion
    for (int i = 0; i < test_strings_count; i++)
        LinkedList_AddTail(&list, (void *)test_strings[i]);

    PrintStrLinkedList(&list);

    // find
    PrintStrLinkedListNode(LinkedList_FindNodeFromHead(&list, (void *)test_strings[list.count - 1]));
    PrintStrLinkedListNode(LinkedList_FindNodeFromTail(&list, (void *)test_strings[0]));
    PrintStrLinkedListNode(LinkedList_IndexFromHead(&list, list.count - 1));
    PrintStrLinkedListNode(LinkedList_IndexFromTail(&list, 0));

    // insertion before/after node
    node = LinkedList_FindNodeFromHead(&list, (void *)test_strings[list.count / 2]);
    puts((const char *)node->elem);

    LinkedList_AddPrev(node, (void *)str_add_prev);
    PrintStrLinkedList(&list);
    LinkedList_AddNext(node, (void *)str_add_next);
    PrintStrLinkedList(&list);

    // deletion
    LinkedList_DelElemFromHead(&list, (void *)test_strings[1]);
    PrintStrLinkedList(&list);
    LinkedList_DelElemFromTail(&list, (void *)test_strings[test_strings_count - 2]);
    PrintStrLinkedList(&list);

    LinkedList_DelHead(&list);
    PrintStrLinkedList(&list);
    LinkedList_DelTail(&list);
    PrintStrLinkedList(&list);

    printf("deleted: %s\n", (const char *)LinkedList_DelNode(node->prev));
    PrintStrLinkedList(&list);
    printf("deleted: %s\n", (const char *)LinkedList_DelNode(node->next));
    PrintStrLinkedList(&list);

    // cleanup
    LinkedList_IterInit(&iter, &list);
    while (LinkedList_IterDel(&iter));

    PrintStrLinkedList(&list);
}

static void PrintStrHashSet(struct HashSet *hashset)
{
    struct HashSet_Iter iter = { };
    const char *str = NULL;

    HashSet_IterInit(&iter, hashset);

    while ((str = HashSet_IterNext(&iter)))
        printf("%s ", str);
    
    printf("| count: %lu\n", hashset->count);
}

static void TestHashSet(void)
{
    struct HashSet hashset = { };
    struct HashSet_Iter iter = { };
    const char *str = NULL;

    puts("- TestHashSet -");

    HashSet_Init(&hashset);

    // add
    for (int i = 0; i < test_strings_count; i++)
        HashSet_Add(&hashset, (void *)test_strings[i], NULL);

    PrintStrHashSet(&hashset);

    // resize
    HashSet_Resize(&hashset, 8);
    PrintStrHashSet(&hashset);

    // find
    HashSet_IterInit(&iter, &hashset);

    for (int i = 0; i < test_strings_count; i++)
        printf("found: %s\n", (const char *)HashSet_Find(&hashset, (void *)test_strings[i]));

    // deletion
    for (int i = test_strings_count - 1; i >= test_strings_count / 2; i--)
        HashSet_Del(&hashset, (void *)test_strings[i]);

    PrintStrHashSet(&hashset);
    
    // cleanup
    while ((str = HashSet_IterDel(&iter)))
        printf("iter_del: %s\n", str);

    HashSet_Cleanup(&hashset);
}

void ContainerTest(void)
{
    TestLinkedList();
    TestHashSet();
}
