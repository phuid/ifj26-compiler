#include "bst.h"
#include <stdlib.h>

int cmp(const void* a, const void*b){
    const int aInt = (*(const int*)a);
    const int bInt = (*(const int*)b);

    return bInt-aInt;
}

void *copy(const void *a){
    int *aCopy = malloc(sizeof(int));
    (*aCopy) = *((int*)a);
    return aCopy;
}

void destroy(void *a){
    free((int*)a);
}


int main(){
    BST *tree = bstCreate(cmp, copy, destroy);
    
    int arr[] = {10, 30, 20};

    for(size_t i = 0; i < sizeof(arr)/sizeof(int); i++){
        bstInsert(tree, &(arr[i]));
    }

    int searchParam = 20;

    const int *res = bstSearch(tree, &searchParam);

    bstRemove(tree, &searchParam);

    bstDestroy(&tree);

    return 0;
}
