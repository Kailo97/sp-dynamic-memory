/**
 * =============================================================================
 * Dynamic memory
 * https://github.com/Kailo97/sp-dynamic-memory
 * 
 * Copyright (C) 2019 Maxim "Kailo" Telezhenko. All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 */

#define DYNAMIC_MEMORY_VERSION "1.1.0"

#define OUT_OF_BOUNDS_SPACE -1

#if !defined HEAP_SIZE
	#define HEAP_SIZE 16384 // sized in cells, 64 KB
#endif

#if !defined BST_ALLOC_COUNT
	#define BST_ALLOC_COUNT 256 // sized in node count
#endif

#if !defined FREE_MEM_SLICE_MIN
	#define FREE_MEM_SLICE_MIN 8 // sized in cells
#endif

#if defined DEBUG_HEAP_MEMORY_FULL
	#if !defined DEBUG_HEAP_MEMORY_INIT
		#define DEBUG_HEAP_MEMORY_INIT
	#endif

	#if !defined DEBUG_HEAP_MEMORY_SIZE
		#define DEBUG_HEAP_MEMORY_SIZE
	#endif

	#if !defined DEBUG_HEAP_MEMORY_BOUNDS
		#define DEBUG_HEAP_MEMORY_BOUNDS
	#endif

	#if !defined DEBUG_HEAP_MEMORY_MAGIC
		#define DEBUG_HEAP_MEMORY_MAGIC
	#endif

	#if !defined DEBUG_HEAP_MEMORY_STATUS
		#define DEBUG_HEAP_MEMORY_STATUS
	#endif
#endif

stock static any heap_space[HEAP_SIZE];

// Reversed in memory from space end
// Binary Search Tree Struct
// {
//     int alloc;
//     int size;
//     int root;
//     Node nodes[alloc];
//     BST Block Header
// }

// Binary Search Tree Node Struct
// {
//     int value;
//     int left;
//     int right;
//     int parent;
// }

// Free Block Struct
// {
//     int prev;
//     int next;
//     int status;
//     int magic_ref; // heap node ref for free / magic for busy
//     int size;
// }

// Busy Block Struct
// {
//     int prev;
//     int next;
//     int status;
//     int magic_ref; // heap node ref for free / magic for busy
//     int size;
//     int data[N];
// }

enum
{
	BSTNode_Value,
	BSTNode_Left,
	BSTNode_Right,
	BSTNode_Parent,

	BSTNode_SIZE,
};

enum
{
	BSTBlockTail_Alloc,
	BSTBlockTail_Size,
	BSTBlockTail_Root,

	BSTBlockTail_SIZE
};

#define BSTBlockTail (HEAP_SIZE - BSTBlockTail_SIZE)

enum
{
	BSTBlockHead_Prev,
	BSTBlockHead_Next,
	BSTBlockHead_Status,

	BSTBlockHead_SIZE
};

#define BST_NULL 0
#define BST_ALLOC_SIZE (BST_ALLOC_COUNT * BSTNode_SIZE)

enum
{
	BlockStatus_Free,
	BlockStatus_Busy,
	BlockStatus_BusyFree, // Not used anymore but not in BST
};

enum
{
	Block_Prev,
	Block_Next,
	Block_Status,
	Block_RefMagic,
	Block_Size,

	Block_SIZE
};

#define BUSY_MEM_MAGIC 0xAAAAAAAA

stock static int BSTBlock_adr;
stock static const char sErrorBSTAlloc[] = "Not enough space for BST allocation: %i";

/*
 * BST, Used for cache free memory blocks
 * 
 * Node1 < Node2 equals:
 * 1) less memory block size
 * 2) if block size equals: less block address
 */
stock static int BST_Minimum(int node)
{
	int n;
	for ( ; ; )
	{
		if ((n = heap_space[node + BSTNode_Left]) == BST_NULL)
		{
			break;
		}
		node = n;
	}
	return node;
}

stock static int BST_Next(int node)
{
	int n;
	if ((n = heap_space[node + BSTNode_Right]) != BST_NULL)
	{
		return BST_Minimum(n);
	}
	n = heap_space[node + BSTNode_Parent];
	while (n != BST_NULL && node == heap_space[n + BSTNode_Right])
	{
		node = n;
		n = heap_space[n + BSTNode_Parent];
	}
	return n;
}

stock static void BST_Remove(int node, bool recursion = false)
{
	// Manage BTS data
	{
		int parent = heap_space[node + BSTNode_Parent],
			left = heap_space[node + BSTNode_Left],
			right = heap_space[node + BSTNode_Right];
		if (left == BST_NULL && right == BST_NULL)
		{
			if (parent != BST_NULL)
			{
				if (heap_space[parent + BSTNode_Left] == node)
				{
					heap_space[parent + BSTNode_Left] = BST_NULL;
				}
				else
				{
					heap_space[parent + BSTNode_Right] = BST_NULL;
				}
			}
			else
			{
				heap_space[BSTBlockTail + BSTBlockTail_Root] = BST_NULL;
			}
		}
		else if (left == BST_NULL || right == BST_NULL)
		{
			if (left == BST_NULL)
			{
				if (parent != BST_NULL)
				{
					if (heap_space[parent + BSTNode_Left] == node)
					{
						heap_space[parent + BSTNode_Left] = right;
					}
					else
					{
						heap_space[parent + BSTNode_Right] = right;
					}
					heap_space[right + BSTNode_Parent] = parent;
				}
				else
				{
					heap_space[BSTBlockTail + BSTBlockTail_Root] = right;
					heap_space[right + BSTNode_Parent] = BST_NULL;
				}
			}
			else
			{
				if (parent != BST_NULL)
				{
					if (heap_space[parent + BSTNode_Left] == node)
					{
						heap_space[parent + BSTNode_Left] = left;
					}
					else
					{
						heap_space[parent + BSTNode_Right] = left;
					}
					heap_space[left + BSTNode_Parent] = parent;
				}
				else
				{
					heap_space[BSTBlockTail + BSTBlockTail_Root] = left;
					heap_space[left + BSTNode_Parent] = BST_NULL;
				}
			}
		}
		else
		{
			int successor = BST_Next(node),
				value = heap_space[node + BSTNode_Value] = heap_space[successor + BSTNode_Value];
			heap_space[value + Block_RefMagic] = node;
			parent = heap_space[successor + BSTNode_Parent];
			if (heap_space[parent + BSTNode_Left] == successor)
			{
				right = heap_space[successor + BSTNode_Right];
				heap_space[parent + BSTNode_Left] = right;
				if (right != BST_NULL)
				{
					heap_space[right + BSTNode_Parent] = parent;
				}
			}
			else
			{
				right = heap_space[successor + BSTNode_Right];
				heap_space[parent + BSTNode_Right] = right;
				if (right != BST_NULL)
				{
					heap_space[right + BSTNode_Parent] = parent;
				}
			}
			node = successor;
		}
	}

	// Swap memory of last element in array and deleted element
	int last = HEAP_SIZE - BSTBlockTail_SIZE - BSTNode_SIZE * heap_space[BSTBlockTail + BSTBlockTail_Size];
	if (node != last)
	{
		int value = heap_space[node + BSTNode_Value] = heap_space[last + BSTNode_Value],
			left = heap_space[node + BSTNode_Left] = heap_space[last + BSTNode_Left],
			right = heap_space[node + BSTNode_Right] = heap_space[last + BSTNode_Right],
			parent = heap_space[node + BSTNode_Parent] = heap_space[last + BSTNode_Parent];
		if (parent == BST_NULL)
		{
			heap_space[BSTBlockTail + BSTBlockTail_Root] = node;
		}
		else
		{
			if (heap_space[parent + BSTNode_Left] == last)
			{
				heap_space[parent + BSTNode_Left] = node;
			}
			else
			{
				heap_space[parent + BSTNode_Right] = node;
			}
		}
		if (left != BST_NULL)
		{
			heap_space[left + BSTNode_Parent] = node;
		}
		if (right != BST_NULL)
		{
			heap_space[right + BSTNode_Parent] = node;
		}
		heap_space[value + Block_RefMagic] = node;
	}

	// Reduce size by 1 and check allocation
	int size = heap_space[BSTBlockTail + BSTBlockTail_Size] -= 1;
	if (recursion == false)
	{
		if (heap_space[BSTBlockTail + BSTBlockTail_Alloc] - size >= BST_ALLOC_COUNT * 3 / 2)
		{
			heap_space[BSTBlockTail + BSTBlockTail_Alloc] -= BST_ALLOC_COUNT;
			int new_head = BSTBlock_adr + BST_ALLOC_SIZE,
				neighbour = heap_space[new_head + BSTBlockHead_Prev] = heap_space[BSTBlock_adr + BSTBlockHead_Prev];
			heap_space[new_head + BSTBlockHead_Next] = heap_space[BSTBlock_adr + BSTBlockHead_Next];
			heap_space[new_head + BSTBlockHead_Status] = heap_space[BSTBlock_adr + BSTBlockHead_Status];


			switch (heap_space[neighbour + Block_Status])
			{
				case BlockStatus_Free:
				{
					BST_Remove(heap_space[neighbour + Block_RefMagic], true);
					heap_space[neighbour + Block_Next] = new_head;
					heap_space[neighbour + Block_Size] += BST_ALLOC_SIZE;
					heap_space[neighbour + Block_RefMagic] = BST_Insert(neighbour);

					heap_space[new_head + BSTBlockHead_Prev] = neighbour;
				}
				case BlockStatus_Busy:
				{
					heap_space[BSTBlock_adr + Block_Next] = new_head;
					heap_space[BSTBlock_adr + Block_Status] = BlockStatus_Free;
					heap_space[BSTBlock_adr + Block_Size] = BST_ALLOC_SIZE - Block_SIZE;
					heap_space[BSTBlock_adr + Block_RefMagic] = BST_Insert(BSTBlock_adr);

					heap_space[new_head + BSTBlockHead_Prev] = BSTBlock_adr;
				}
				case BlockStatus_BusyFree:
				{
					int free_space = BST_ALLOC_SIZE + heap_space[neighbour + Block_Size],
						prev = heap_space[neighbour + Block_Prev];
					while (heap_space[prev + Block_Status] == BlockStatus_BusyFree)
					{
						neighbour = prev;
						free_space += Block_SIZE + heap_space[neighbour + Block_Size];
						prev = heap_space[neighbour + Block_Prev];
					}

					heap_space[neighbour + Block_Next] = new_head;
					heap_space[neighbour + Block_Status] = BlockStatus_Free;
					heap_space[neighbour + Block_Size] = free_space;
					heap_space[neighbour + Block_RefMagic] = BST_Insert(neighbour);

					heap_space[new_head + BSTBlockHead_Prev] = neighbour;
				}
			}
			heap_space[new_head + BSTBlockHead_Next] = OUT_OF_BOUNDS_SPACE;
			heap_space[new_head + BSTBlockHead_Status] = BlockStatus_Busy;
			BSTBlock_adr = new_head;
		}
	}
}

stock static int BST_Insert(int value)
{
	if (heap_space[BSTBlockTail + BSTBlockTail_Root] == BST_NULL)
	{
		int node = HEAP_SIZE - BSTBlockTail_SIZE - BSTNode_SIZE;
		heap_space[node + BSTNode_Value] = value;
		heap_space[node + BSTNode_Left] = BST_NULL;
		heap_space[node + BSTNode_Right] = BST_NULL;
		heap_space[node + BSTNode_Parent] = BST_NULL;
		heap_space[BSTBlockTail + BSTBlockTail_Root] = node;
		heap_space[BSTBlockTail + BSTBlockTail_Size] = 1;
		return node;
	}

	if (heap_space[BSTBlockTail + BSTBlockTail_Alloc] <= heap_space[BSTBlockTail + BSTBlockTail_Size])
	{
		int prev = heap_space[BSTBlock_adr + BSTBlockHead_Prev];
		if (heap_space[prev + Block_Status] != BlockStatus_Free)
		{
			SetFailState(sErrorBSTAlloc, 1);
		}
		int size = heap_space[prev + Block_Size];
		if (size < BST_ALLOC_SIZE)
		{
			SetFailState(sErrorBSTAlloc, 2);
		}
		BST_Remove(heap_space[prev + Block_RefMagic]);
		int new_head = BSTBlock_adr - BST_ALLOC_SIZE;
		heap_space[new_head + BSTBlockHead_Prev] = heap_space[BSTBlock_adr + BSTBlockHead_Prev];
		heap_space[new_head + BSTBlockHead_Next] = OUT_OF_BOUNDS_SPACE;
		heap_space[new_head + BSTBlockHead_Status] = BlockStatus_Busy;
		heap_space[BSTBlockTail + BSTBlockTail_Alloc] += BST_ALLOC_COUNT;
		heap_space[prev + Block_Size] -= BST_ALLOC_SIZE;
		heap_space[prev + Block_RefMagic] = BST_Insert(prev);
		heap_space[prev + Block_Next] = BSTBlock_adr = new_head;
	}

	int node = HEAP_SIZE - BSTBlockTail_SIZE - BSTNode_SIZE * (++heap_space[BSTBlockTail + BSTBlockTail_Size]);
	heap_space[node + BSTNode_Value] = value;
	heap_space[node + BSTNode_Left] = BST_NULL;
	heap_space[node + BSTNode_Right] = BST_NULL;

	// cache size for multiple access
	int size = heap_space[value + Block_Size];

	int x = heap_space[BSTBlockTail + BSTBlockTail_Root], c, b;
	for ( ; ; )
	{
		if (size != (c = heap_space[(b = heap_space[x + BSTNode_Value]) + Block_Size]) ? size > c : value > b)
		{
			if ((c = heap_space[x + BSTNode_Right]) != BST_NULL)
			{
				x = c;
			}
			else
			{
				heap_space[x + BSTNode_Right] = node;
				heap_space[node + BSTNode_Parent] = x;

				break;
			}
		}
		else
		{
			if ((c = heap_space[x + BSTNode_Left]) != BST_NULL)
			{
				x = c;
			}
			else
			{
				heap_space[x + BSTNode_Left] = node;
				heap_space[node + BSTNode_Parent] = x;

				break;
			}
		}
	}

	return node;
}

stock static int BST_FindEnough(int size)
{
	int node = heap_space[BSTBlockTail + BSTBlockTail_Root];
	if (node == BST_NULL)
	{
		return OUT_OF_BOUNDS_SPACE;
	}

	int value = heap_space[node + BSTNode_Value];
	while (heap_space[value + Block_Size] < size)
	{
		node = heap_space[node + BSTNode_Right];
		if (node == BST_NULL)
		{
			return OUT_OF_BOUNDS_SPACE;
		}
		value = heap_space[node + BSTNode_Value];
	}

	int child;
	for ( ; ; )
	{
		child = heap_space[node + BSTNode_Left];
		if (child == BST_NULL)
		{
			break;
		}
		value = heap_space[child + BSTNode_Value];
		if (heap_space[value + Block_Size] < size)
		{
			break;
		}
		node = child;
	}

	return heap_space[node + BSTNode_Value];
}

stock static bool inited = false;
stock static const char sErrorNotInit[] = "Memory not initialized";
stock static const char sErrorAddress[] = "Address is invalid: %i";
stock static const char sErrorInvSize[] = "Invalid size %i";
stock static const char sErrorNotEnough[] = "Not enough space to allocate";
stock static const char sErrorInvMagic[] = "Invalid magic value";
stock static const char sErrorNotBusy[] = "Accessed memory is not busy";

stock int memalloc(int size)
{
	static const char s_dm_version[] = DYNAMIC_MEMORY_VERSION;
	#pragma unused s_dm_version

	if (!inited)
	{
		// Construct BST
		heap_space[BSTBlockTail + BSTBlockTail_Alloc] = BST_ALLOC_COUNT;
		heap_space[BSTBlockTail + BSTBlockTail_Size] = 0;
		heap_space[BSTBlockTail + BSTBlockTail_Root] = BST_NULL;
		BSTBlock_adr = HEAP_SIZE - (BSTBlockHead_SIZE + BST_ALLOC_SIZE + BSTBlockTail_SIZE);
		heap_space[BSTBlock_adr + BSTBlockHead_Prev] = Block_SIZE;
		heap_space[BSTBlock_adr + BSTBlockHead_Next] = OUT_OF_BOUNDS_SPACE;
		heap_space[BSTBlock_adr + BSTBlockHead_Status] = BlockStatus_Busy;

		// Construct left border block
		heap_space[Block_Prev] = OUT_OF_BOUNDS_SPACE;
		heap_space[Block_Next] = Block_SIZE;
		heap_space[Block_Status] = BlockStatus_Busy;
		heap_space[Block_Size] = 0;
		heap_space[Block_RefMagic] = 0;

		// Construct 0-Block
		heap_space[Block_SIZE + Block_Prev] = 0;
		heap_space[Block_SIZE + Block_Next] = BSTBlock_adr;
		heap_space[Block_SIZE + Block_Status] = BlockStatus_Free;
		heap_space[Block_SIZE + Block_Size] = HEAP_SIZE - (BSTBlockHead_SIZE + BST_ALLOC_SIZE + BSTBlockTail_SIZE) - Block_SIZE * 2;
		heap_space[Block_SIZE + Block_RefMagic] = BST_Insert(Block_SIZE);

		inited = true;
	}

#if defined DEBUG_HEAP_MEMORY_SIZE
	if (size <= 0)
		ThrowError(sErrorInvSize, size);
#endif

	int block = BST_FindEnough(size);

	if (block == OUT_OF_BOUNDS_SPACE)
		ThrowError(sErrorNotEnough);

	heap_space[block + Block_Status] = BlockStatus_Busy;
	BST_Remove(heap_space[block + Block_RefMagic]);

	heap_space[block + Block_RefMagic] = BUSY_MEM_MAGIC;

	// if splitted
	int space = heap_space[block + Block_Size];
	if (space >= size + Block_SIZE + FREE_MEM_SLICE_MIN)
	{
		heap_space[block + Block_Size] = size;

		int shard = block + Block_SIZE + size;
		heap_space[shard+Block_Status] = BlockStatus_Free;
		heap_space[shard+Block_Size] = space - (size + Block_SIZE);
		heap_space[shard+Block_RefMagic] = BST_Insert(shard);

		int neighbour = heap_space[block+Block_Next];

		heap_space[block+Block_Next] = shard;
		heap_space[shard+Block_Prev] = block;
		heap_space[shard+Block_Next] = neighbour;
		heap_space[neighbour+Block_Prev] = shard;
	}

	return block + Block_SIZE;
}

stock void free(int adr)
{
#if defined DEBUG_HEAP_MEMORY_INIT
	if (!inited)
		ThrowError(sErrorNotInit);
#endif

#if defined DEBUG_HEAP_MEMORY_BOUNDS
	if (adr < Block_SIZE * 2 || adr >= BSTBlock_adr)
		ThrowError(sErrorAddress, adr);
#endif

	int block = adr - Block_SIZE;

#if defined DEBUG_HEAP_MEMORY_MAGIC
	if (heap_space[block + Block_RefMagic] != BUSY_MEM_MAGIC)
		ThrowError(sErrorInvMagic);
#endif

#if defined DEBUG_HEAP_MEMORY_STATUS
	if (heap_space[block + Block_Status] != BlockStatus_Busy)
		ThrowError(sErrorNotBusy);
#endif

	int size = heap_space[block + Block_Size],
		prev = heap_space[block + Block_Prev],
		next = heap_space[block + Block_Next];

	// TODO: memory fragmentation, make block BusyFree (not yet in BST, but not used anymore)
	//if (size < FREE_MEM_SLICE_MIN && heap_space[prev + Block_Status] && heap_space[next + Block_Status])
	//{
	//	
	//}

	switch (heap_space[prev + Block_Status])
	{
		case BlockStatus_Free:
		{
			BST_Remove(heap_space[prev + Block_RefMagic]);
			size += heap_space[prev + Block_Size] + Block_SIZE;
			block = prev;
		}
		case BlockStatus_Busy:
		{
			heap_space[block + Block_Status] = BlockStatus_Free;
		}
		case BlockStatus_BusyFree:
		{
			do
			{
				size += heap_space[prev + Block_Size] + Block_SIZE;
				block = prev;
				prev = heap_space[block + Block_Prev];
			} while (heap_space[prev + Block_Status] == BlockStatus_BusyFree);
			heap_space[block + Block_Status] = BlockStatus_Free;
		}
	}

	switch (heap_space[next + Block_Status])
	{
		case BlockStatus_Free:
		{
			BST_Remove(heap_space[next + Block_RefMagic]);
			size += heap_space[next + Block_Size] + Block_SIZE;
			next = heap_space[next + Block_Next];
		}
		case BlockStatus_BusyFree:
		{
			do
			{
				size += heap_space[next + Block_Size] + Block_SIZE;
				next = heap_space[next + Block_Next];
			}
			while (heap_space[prev + Block_Status] == BlockStatus_BusyFree);
		}
	}

	heap_space[block + Block_Next] = next;
	heap_space[next + Block_Prev] = block;

	heap_space[block + Block_Size] = size;
	heap_space[block + Block_RefMagic] = BST_Insert(block);
}

stock int realloc(int adr, int size)
{
#if defined DEBUG_HEAP_MEMORY_INIT
	if (!inited)
		ThrowError(sErrorNotInit);
#endif

#if defined DEBUG_HEAP_MEMORY_BOUNDS
	if (adr < Block_SIZE * 2 || adr >= BSTBlock_adr)
		ThrowError(sErrorAddress, adr);
#endif

#if defined DEBUG_HEAP_MEMORY_SIZE
	if (size <= 0)
		ThrowError(sErrorInvSize, size);
#endif

	int block = adr - Block_SIZE;

#if defined DEBUG_HEAP_MEMORY_MAGIC
	if (heap_space[block + Block_RefMagic] != BUSY_MEM_MAGIC)
		ThrowError(sErrorInvMagic);
#endif

#if defined DEBUG_HEAP_MEMORY_STATUS
	if (heap_space[block + Block_Status] != BlockStatus_Busy)
		ThrowError(sErrorNotBusy);
#endif

	int space = heap_space[block + Block_Size],
		next = heap_space[block + Block_Next];

	if (space >= size)
	{
		if (space == size)
			return adr;

		switch (heap_space[next + Block_Status])
		{
			case BlockStatus_Free:
			{
				BST_Remove(heap_space[next + Block_RefMagic]);
				int new_next = block + Block_SIZE + size;

				heap_space[block + Block_Size] = size;

				heap_space[new_next + Block_Prev] = block;
				int next_next = heap_space[new_next + Block_Next] = heap_space[next + Block_Next];
				heap_space[new_next + Block_Status] = BlockStatus_Free;
				heap_space[new_next + Block_Size] = heap_space[next + Block_Size] + (next - new_next);
				heap_space[new_next + Block_RefMagic] = BST_Insert(new_next);

				heap_space[block + Block_Next] = new_next;
				heap_space[next_next + Block_Prev] = new_next;
			}
			case BlockStatus_Busy:
			{
				if (space >= size + Block_SIZE + FREE_MEM_SLICE_MIN)
				{
					heap_space[block + Block_Size] = size;

					int shard = block + Block_SIZE + size;
					heap_space[shard+Block_Status] = BlockStatus_Free;
					heap_space[shard+Block_Size] = space - (size + Block_SIZE);
					heap_space[shard+Block_RefMagic] = BST_Insert(shard);

					int neighbour = heap_space[block+Block_Next];

					heap_space[block+Block_Next] = shard;
					heap_space[shard+Block_Prev] = block;
					heap_space[shard+Block_Next] = neighbour;
					heap_space[neighbour+Block_Prev] = shard;
				}
			}			
			case BlockStatus_BusyFree:
			{
				int free_space = space - size,
					new_next = block + Block_SIZE + size;

				do
				{
					free_space += heap_space[next + Block_Size] + Block_SIZE;
					next = heap_space[next + Block_Next];
				}
				while (heap_space[next + Block_Status] == BlockStatus_Busy);

				heap_space[block + Block_Size] = size;
				heap_space[block + Block_Next] = new_next;
				heap_space[new_next + Block_Prev] = block;
				heap_space[new_next + Block_Next] = next;
				heap_space[next + Block_Prev] = new_next;
				heap_space[new_next + Block_Status] = BlockStatus_Free;
				heap_space[new_next + Block_Size] = free_space - Block_SIZE;
				heap_space[new_next + Block_RefMagic] = BST_Insert(new_next);
			}
		}

		return adr;
	}

	if (heap_space[next + Block_Status] == BlockStatus_Free)
	{
		int delta = size - space;
		if (Block_SIZE + heap_space[next + Block_Size] >= delta)
		{
			BST_Remove(heap_space[next + Block_RefMagic]);

			int next_size = heap_space[next + Block_Size],
				next_next = heap_space[next + Block_Next];

			if (next_size >= delta + FREE_MEM_SLICE_MIN)
			{
				int new_next = next + delta;

				heap_space[block + Block_Size] = size;
				heap_space[new_next + Block_Size] = next_size - delta;
				heap_space[new_next + Block_Status] = BlockStatus_Free;
				heap_space[new_next + Block_Next] = next_next;
				heap_space[next_next + Block_Prev] = new_next;
				heap_space[new_next + Block_Prev] = block;
				heap_space[block + Block_Next] = new_next;
				heap_space[new_next + Block_RefMagic] = BST_Insert(new_next);
			}
			else
			{
				heap_space[block + Block_Size] += next_size + Block_SIZE;
				heap_space[next_next + Block_Prev] = block;
				heap_space[block + Block_Next] = next_next;
			}

			return adr;
		}
	}

	free(adr);
	int new_adr = memalloc(size);
	if (new_adr < adr)
		memmove(adr, new_adr, space);
	else if (new_adr > adr)
		memmover(adr, new_adr, space);

	return new_adr;
}

stock void store(int adr, any value)
{
#if defined DEBUG_HEAP_MEMORY_INIT
	if (!inited)
		ThrowError(sErrorNotInit);
#endif

#if defined DEBUG_HEAP_MEMORY_BOUNDS
	if (adr < Block_SIZE * 2 || adr >= BSTBlock_adr)
		ThrowError(sErrorAddress, adr);
#endif

	heap_space[adr] = value;
}

stock any load(int adr)
{
#if defined DEBUG_HEAP_MEMORY_INIT
	if (!inited)
		ThrowError(sErrorNotInit);
#endif

#if defined DEBUG_HEAP_MEMORY_BOUNDS
	if (adr < Block_SIZE * 2 || adr >= BSTBlock_adr)
		ThrowError(sErrorAddress, adr);
#endif

	return heap_space[adr];
}

stock void mempaste(int adr, const any[] array, int size)
{
#if defined DEBUG_HEAP_MEMORY_INIT
	if (!inited)
		ThrowError(sErrorNotInit);
#endif

#if defined DEBUG_HEAP_MEMORY_SIZE
	if (size <= 0)
		ThrowError(sErrorInvSize, size);
#endif

#if defined DEBUG_HEAP_MEMORY_BOUNDS
	if (adr < Block_SIZE * 2 || adr + size >= BSTBlock_adr)
		ThrowError(sErrorAddress, adr);
#endif

	for (int i = 0; i < size; i++)
		heap_space[adr + i] = array[i];
}

stock void memcopy(int adr, any[] array, int size)
{
#if defined DEBUG_HEAP_MEMORY_INIT
	if (!inited)
		ThrowError(sErrorNotInit);
#endif

#if defined DEBUG_HEAP_MEMORY_SIZE
	if (size <= 0)
		ThrowError(sErrorInvSize, size);
#endif

#if defined DEBUG_HEAP_MEMORY_BOUNDS
	if (adr < Block_SIZE * 2 || adr + size >= BSTBlock_adr)
		ThrowError(sErrorAddress, adr);
#endif

	for (int i = 0; i < size; i++)
		array[i] = heap_space[adr + i];
}

stock void memmove(int fromadr, int toadr, int size)
{
#if defined DEBUG_HEAP_MEMORY_INIT
	if (!inited)
		ThrowError(sErrorNotInit);
#endif

#if defined DEBUG_HEAP_MEMORY_SIZE
	if (size <= 0)
		ThrowError(sErrorInvSize, size);
#endif

#if defined DEBUG_HEAP_MEMORY_BOUNDS
	if (fromadr < Block_SIZE * 2 || fromadr + size >= BSTBlock_adr)
		ThrowError(sErrorAddress, fromadr);

	if (toadr < Block_SIZE * 2 || toadr + size >= BSTBlock_adr)
		ThrowError(sErrorAddress, toadr);
#endif

	for (int i = 0; i < size; i++)
		heap_space[toadr + i] = heap_space[fromadr + i];
}

stock void memmover(int fromadr, int toadr, int size)
{
#if defined DEBUG_HEAP_MEMORY_INIT
	if (!inited)
		ThrowError(sErrorNotInit);
#endif

#if defined DEBUG_HEAP_MEMORY_SIZE
	if (size <= 0)
		ThrowError(sErrorInvSize, size);
#endif

#if defined DEBUG_HEAP_MEMORY_BOUNDS
	if (fromadr < Block_SIZE * 2 || fromadr + size >= BSTBlock_adr)
		ThrowError(sErrorAddress, fromadr);

	if (toadr < Block_SIZE * 2 || toadr + size >= BSTBlock_adr)
		ThrowError(sErrorAddress, toadr);
#endif

	for (int i = size - 1; i >= 0; i--)
		heap_space[toadr + i] = heap_space[fromadr + i];
}

stock static void bytemove(const char[] from, char[] to, int maxlength)
{
	int i = 0, c;
	while ((c = from[i]) != 0 && --maxlength)
	{
		to[i] = c;
		i++;
	}
	to[i] = '\0';
}

stock void mempastestr(int adr, const char[] str, int size)
{
#if defined DEBUG_HEAP_MEMORY_INIT
	if (!inited)
		ThrowError(sErrorNotInit);
#endif

#if defined DEBUG_HEAP_MEMORY_SIZE
	if (size <= 0)
		ThrowError(sErrorInvSize, size);
#endif

#if defined DEBUG_HEAP_MEMORY_BOUNDS
	if (adr < Block_SIZE * 2 || adr + (size + 3) / 4 >= BSTBlock_adr)
		ThrowError(sErrorAddress, adr);
#endif

	bytemove(str, view_as<char>(heap_space[adr]), size);
}

stock void memcopystr(int adr, char[] str, int size)
{
#if defined DEBUG_HEAP_MEMORY_INIT
	if (!inited)
		ThrowError(sErrorNotInit);
#endif

#if defined DEBUG_HEAP_MEMORY_SIZE
	if (size <= 0)
		ThrowError(sErrorInvSize, size);
#endif

#if defined DEBUG_HEAP_MEMORY_BOUNDS
	if (adr < Block_SIZE * 2 || adr + (size + 3) / 4 >= BSTBlock_adr)
		ThrowError(sErrorAddress, adr);
#endif

	bytemove(view_as<char>(heap_space[adr]), str, size);
}

stock void DumpMemory()
{
	PrintToServer("Memory blocks dump:");

	int block = Block_SIZE;
	while (block != BSTBlock_adr)
	{
		PrintToServer("%i: status %i, size %i, ref %i", block, heap_space[block + Block_Status], heap_space[block + Block_Size], heap_space[block + Block_RefMagic]);
		block = heap_space[block + Block_Next];
	}
	int size = heap_space[BSTBlockTail + BSTBlockTail_Size];
	PrintToServer("BST %i: size %i, alloc %i, root %i", BSTBlock_adr, size, heap_space[BSTBlockTail + BSTBlockTail_Alloc], heap_space[BSTBlockTail + BSTBlockTail_Root]);
	for (int i = 1; i <= size; i++)
	{
		block = HEAP_SIZE - BSTBlockTail_SIZE - (i * BSTNode_SIZE);
		PrintToServer("%i: child %i %i, parent %i, value %i", block, heap_space[block + BSTNode_Left], heap_space[block + BSTNode_Right], heap_space[block + BSTNode_Parent], heap_space[block + BSTNode_Value]);
	}
}
