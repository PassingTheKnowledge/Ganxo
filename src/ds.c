#include <ganxo.h>

//--------------------------------------------------------------------------
bool GANXO_API gnx_singly_list_remove(
    gnx_singly_list_item_t *head,
    gnx_singly_list_item_t *entry)
{
    gnx_singly_list_item_t *last = head;
    // Find the node
    for (gnx_singly_list_item_t *cur = head->next; cur != NULL; cur = cur->next)
    {
        if (cur == entry)
        {
            // Link previous node with next node (unlink current node)
            last->next = cur->next;
            return true;
        }
        last = cur;
    }

    return false;
}
