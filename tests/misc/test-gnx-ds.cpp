namespace singly_list_test
{
//--------------------------------------------------------------------------
struct my_struct
{
    char id;
    GNX_SINGLY_LIST_ITEM_DEFINE;
    int a;
    my_struct(char a)
    {
        static int id = 0;
        this->id = ++id;
        this->a = a;
    }
};

//--------------------------------------------------------------------------
void create(gnx_singly_list_item_t *head)
{
    gnx_singly_list_init(head);
    GNX_SINGLY_LIST_ITEM_PUSH(
        head,
        new my_struct('A'));

    GNX_SINGLY_LIST_ITEM_PUSH(
        head,
        new my_struct('B'));

    GNX_SINGLY_LIST_ITEM_PUSH(
        head,
        new my_struct('C'));

    GNX_SINGLY_LIST_ITEM_PUSH(
        head,
        new my_struct('D'));
}

//--------------------------------------------------------------------------
void test_push_pop()
{
    GNX_SINGLY_LIST_DECLARE(head);
    create(&head);
    auto s = GNX_SINGLY_LIST_ITEM_POP(&head, my_struct);

    s = GNX_SINGLY_LIST_ITEM_POP(&head, my_struct);
    s = GNX_SINGLY_LIST_ITEM_POP(&head, my_struct);
    s = GNX_SINGLY_LIST_ITEM_POP(&head, my_struct);
}

//--------------------------------------------------------------------------
void iterate(gnx_singly_list_item_t *head)
{
    for (auto cur = head->next; cur != NULL; cur = cur->next)
    {
        my_struct *s = GNX_SINGLY_LIST_RECORD(cur, my_struct);
        printf("id = %c\n", s->a);
    }
}

//--------------------------------------------------------------------------
void delete_all(gnx_singly_list_item_t *head)
{
    for (auto cur = head->next; cur != NULL; )
    {
        my_struct *s = GNX_SINGLY_LIST_RECORD(cur, my_struct);
        cur = cur->next;
        delete s;
    }
    head->next = NULL;
}

//--------------------------------------------------------------------------
void test_push_remove()
{
    GNX_SINGLY_LIST_DECLARE(head);
    create(&head);

    // Initial list
    printf("Initial list:\n");
    iterate(&head);

    // Remove the last entry
    auto entry = head.next;
    my_struct *s = GNX_SINGLY_LIST_RECORD(
        entry, 
        my_struct);

    gnx_singly_list_remove(
        &head, 
        entry);
    delete s;

    // Remove the first entry
    printf("After last removal:\n");
    iterate(&head);

    // Get the first item
    entry = head.next->next->next;
    s = GNX_SINGLY_LIST_RECORD(
        entry,
        my_struct);
    gnx_singly_list_remove(
        &head,
        entry);

    printf("After head removal:\n");
    iterate(&head);
    delete_all(&head);
    iterate(&head);

    // Re-create anew
    printf("Recreated list:\n");
    create(&head);
    iterate(&head);

    printf("Remove from the middle:\n");
    entry = head.next->next;
    s = GNX_SINGLY_LIST_RECORD(
        entry,
        my_struct);

    gnx_singly_list_remove(
        &head,
        entry
    );
    printf("s=%c\n", s->a);
    delete s;

    iterate(&head);
}

} // namespace