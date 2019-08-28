# Dynamic memory for SourcePawn
This include file provide functions to create and use statically allocated memory heap inside your plugin.
Heap is based on global one-dimensional array.
## Download
From [source code above](https://github.com/Kailo97/sp-dynamic-memory/blob/master/memory.inc) or [Releases page](https://github.com/Kailo97/sp-dynamic-memory/releases).
## Usage
```sourcepawn
// You need to use this defines before including
#define HEAP_SIZE 16384 // optional define, to override heap size; sized in cells
#define BST_ALLOC_COUNT 256 // optional define, to override BST allocation step size; sized in node count (number of objects)
#define FREE_MEM_SLICE_MIN 8 // optional define, to override memory sliceing limitation

#define DEBUG_HEAP_MEMORY_FULL // define it, to turn on all validation checks
#define DEBUG_HEAP_MEMORY_INIT // define it, to turn on heap initialization check
#define DEBUG_HEAP_MEMORY_SIZE // define it, to turn on passed size validation
#define DEBUG_HEAP_MEMORY_BOUNDS // define it, to turn on passed address validation
#define DEBUG_HEAP_MEMORY_MAGIC // define it, to turn on magic value validation
#define DEBUG_HEAP_MEMORY_STATUS // define it, to turn on status validation

// Use 0 as null pointer (address)
// Address represent an integral value above 0 (array cell index, from where allocated memory started)

int memalloc(int size);
void free(int adr);
int realloc(int adr, int size);

void store(int adr, any value);
any load(int adr);
void mempaste(int adr, const any[] array, int size);
void memcopy(int adr, any[] array, int size);
void memmove(int fromadr, int toadr, int size);
void memmover(int fromadr, int toadr, int size);
```
## Links
[AlliedModders Forum Page][1]
[HLmod Forum Page][2]
Support me: [![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)][3] or [![Donate](https://img.shields.io/badge/Donate-Qiwi-green.svg)][4]

[1]: / 
[2]: / 
[3]: https://www.paypal.me/KailoTM
[4]: https://qiwi.me/kailo
