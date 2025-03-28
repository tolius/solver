#include "watkinssolution.h"

#include <assert.h>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#define open _open
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#include <nmmintrin.h> // Intel and Microsoft header for _mm_popcnt_u64()
#endif


//using namespace std;
namespace fs = std::filesystem;


static constexpr uint32_t HASHTABLE_MASK = 0xfffff;


int bb_popcount(uint64_t bb)
{
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
	return (int)_mm_popcnt_u64(bb);
#else // Assumed gcc or compatible compiler
	return __builtin_popcountll(bb);
#endif
}

bool arr_get_bit(const std::vector<uint64_t>& arr, size_t n)
{
	return !!(arr[n >> 6] & (size_t(1) << (n & 0x3f)));
}

void arr_set_bit(std::vector<uint64_t>& arr, size_t n)
{
	arr[n >> 6] |= (size_t(1) << (n & 0x3f));
}

uint32_t compute_hash(uint32_t n)
{
	uint32_t k = n * n;
	k += (n >> 10) ^ ((~n) & 0x3ff);
	k += ((n >> 5) ^ ((~n) & 0x7fff)) << 5;
	k += (((~n) >> 15) ^ (n & 0x1f)) << 5;
	k += (n >> 4) & 0x55aa55;
	k += ((~n) >> 8) & 0xaa55aa;
	return k & HASHTABLE_MASK;
}

move_t node_move(const node_t* node)
{
	// 76543210 76543210
	//			111111 from
	//	 1111 11	   to
	// 01110000 00000000 7 << 12
	// 11110000 00000000 0xf000
	// 10000000 00000000 ep
	// 01111111 11111111 0x7fff

	uint16_t move = node->move;
	if (move == 0xfedc || move == 0xedc)
		return 0;

	//  hack for unsolved proof
	if ((move >> 12) > 8 || (move >> 12) == 7)
		move ^= 0xf000;

	// strip ep mask
	move &= 0x7fff;

	return move;
}

bool node_is_trans(const node_t* node)
{
	return (node->data & (1U << 31)) == (1U << 31);
}

bool node_trans_and_sibling(const node_t* node)
{
	return (node->data & (3U << 30)) == (3U << 30);
}

uint32_t node_trans_index(const node_t* node)
{
	return node->data & 0x3fffffff;
}

bool node_has_child(const node_t* node)
{
	return ((node->data & (3U << 30)) == (1U << 30)) && ((node->data & 0x3fffffff) != 0x3fffffff);
}

const node_t* WatkinsTree::next(const node_t* node) const
{
	if (root == node)
		return nodes;
	else
		return node + 1;
}

node_t* WatkinsTree::from_index(uint32_t index) const
{
	if (!index)
		return nullptr;
	else
		return nodes + index - 1;
}

uint32_t WatkinsTree::node_index(const node_t* node) const
{
	if (root == node)
		return 0;
	else
		return node - nodes + 1;
}

const node_t* WatkinsTree::trans(const node_t* node) const
{
	return from_index(node_trans_index(node));
}

const node_t* WatkinsTree::trans_ns(const node_t* node) const
{
	if ((node->data & 0x3fffffff) != 0x3fffffff)
		return from_index(node->data & 0x3fffffff);
	else
		return ((node->data & (1U << 30)) == (1U << 30)) ? next(node) : nullptr;
}

const node_t* WatkinsTree::next_sibling(const node_t* node) const
{
	if (node_trans_and_sibling(node))
		return next(node);
	else
		return node_is_trans(node) ? nullptr : trans_ns(node);
}

uint32_t WatkinsTree::lookup_subtree_size(const node_t* node) const
{
	while (node_is_trans(node))
		node = trans(node);

	uint32_t index = node_index(node);
	uint32_t bucket = compute_hash(index);
	while (hashtable[bucket].index)
	{
		if (index == hashtable[bucket].index)
			return hashtable[bucket].size;
		bucket = (bucket + 1) & HASHTABLE_MASK;
	}

	return 0;
}

bool WatkinsTree::save_subtree_size(const node_t* node, uint32_t size)
{
	if (num_hash_entries > HASHTABLE_MASK / 8)
	{
		// do not fill table too much
		return false;
	}

	while (node_is_trans(node))
		node = trans(node);

	uint32_t bucket = compute_hash(node_index(node));
	while (hashtable[bucket].index)
		bucket = (bucket + 1) & HASHTABLE_MASK;

	hashtable[bucket].index = node_index(node);
	hashtable[bucket].size = size;
	num_hash_entries++;

	if (!node_has_child(node))
		return true;

	if (!next_sibling(next(node)))
		return save_subtree_size(next(node), (size > 0) ? (size - 1) : 0);

	return true;
}

const node_t* WatkinsTree::get_move(move_t move, const node_t* node) const
{
	if (!node)
		return nullptr;
	if (!node_has_child(node))
		return nullptr;

	const node_t* child = next(node);

	do
	{
		if (node_move(child) == move)
		{
			while (node_is_trans(child))
				child = trans(child);
			return child;
		}
	} while (child = next_sibling(child));

	return nullptr;
}

void WatkinsTree::walk(const node_t* node, bool transpositions)
{
	uint32_t index = node_index(node);
	uint16_t k = node->move >> 12;
	assert(node_trans_index(node) != 0x3fffffff);
	assert(node->move == 0xfedc || (k != 7 && k < 9));

	if (transpositions)
	{
		if (arr_get_bit(arr, index))
			return;
		arr_set_bit(arr, index);
	}

	if (node_is_trans(node))
	{
		walk(trans(node), transpositions);
		return;
	}

	if (!transpositions)
	{
		if (arr_get_bit(arr, index))
			return;
		arr_set_bit(arr, index);
	}

	if (!node_has_child(node))
		return;

	const node_t* child = next(node);
	do
	{
		walk(child, transpositions);
	} while (child = next_sibling(child));
}

uint32_t WatkinsTree::get_subtree_size(const node_t* node)
{
	if (root == node)
		return tree_size;

	uint16_t k = node->move >> 12;
	assert(node_trans_index(node) != 0x3fffffff);
	assert(node->move == 0xfedc || (k != 7 && k < 9)); // or it can be an incomplete solution like 1.Na3 e6 w/o 2.b3

	while (node_is_trans(node))
		node = trans(node);

	uint32_t subtree_size = lookup_subtree_size(node);
	if (subtree_size)
		return subtree_size;

	uint32_t size = (tree_size + 63) / 64;
	fill_n(arr.begin(), size, 0);
	walk(node, false);

	for (uint32_t i = 0; i < size; i++)
	{
		subtree_size += bb_popcount(arr[i]);
	}

	save_subtree_size(node, subtree_size);
	return subtree_size;
}

void query_result_add(query_result_t* result, move_t move, uint32_t size)
{
	for (auto& mb : *result)
	{
		if (mb.move == move)
		{
			mb.size += size;
			return;
		}
	}

	result->emplace_back(move, size);
}

bool WatkinsTree::query_children(const node_t* node, query_result_t* result)
{
	assert(node);

	if (!node_has_child(node))
		return false;

	const node_t* child = next(node);

	do
	{
		query_result_add(result, node_move(child), get_subtree_size(child));
	} while (child = next_sibling(child));

	return true;
}

bool WatkinsTree::query_children_fast(const node_t* node, query_result_t* result)
{
	assert(node);

	if (!node_has_child(node))
		return false;

	const node_t* child = next(node);

	do
	{
		query_result_add(result, node_move(child), 1);
	} while (child = next_sibling(child));

	return true;
}

bool WatkinsTree::query(const move_t* moves, size_t moves_len, query_result_t* result)
{
	if (prolog_len > moves_len)
	{
		for (size_t i = 0; i < moves_len; i++)
		{
			if (prolog[i] != moves[i])
				return false;
		}

		query_result_add(result, prolog[moves_len], tree_size + prolog_len - moves_len);
		return true;
	}

	for (size_t i = 0; i < prolog_len; i++)
	{
		if (prolog[i] != moves[i])
			return false;
	}

	const node_t* node = root;
	for (size_t i = prolog_len; i < moves_len; i++)
	{
		node = get_move(moves[i], node);
		if (!node)
			return false;
	}

	return query_children(node, result);
}


WatkinsTree::WatkinsTree()
{}

WatkinsTree::~WatkinsTree()
{
	if (is_open())
		close_tree();
}

bool WatkinsTree::open_tree(const std::filesystem::path& filepath)
{
	if (is_open())
		close_tree();

	fd = open(filepath.generic_string().c_str(), O_RDONLY);
	if (fd == -1)
		return false;

	uintmax_t filesize = fs::file_size(filepath);
	if (filesize == 0)
		return false;

#ifdef _WIN32
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (h == INVALID_HANDLE_VALUE)
		return nullptr;

	DWORD size_high;
	DWORD size_low = GetFileSize(h, &size_high);
	mapping = CreateFileMapping(h, nullptr, PAGE_READONLY, size_high, size_low, nullptr);
	CloseHandle(h);
	if (!mapping)
		return nullptr;

	root = (node_t*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
#else
	root = (node_t*)mmap(NULL, filesize, PROT_READ, MAP_SHARED, fd, 0);
	const size_t page_size = sysconf(_SC_PAGE_SIZE);
	num_pages = filesize / page_size;
	if (filesize % page_size > 0)
		num_pages++;
#endif

	prolog_len = root->move;
	prolog = (move_t*)(root + 1);
	nodes = (node_t*)(prolog + prolog_len);

	tree_size = node_trans_index(root);
	if (!tree_size)
	{
		close_tree();
		return false;
	}

	arr = std::vector<uint64_t>((tree_size / 8 + 64) / sizeof(uint64_t));

	num_hash_entries = 0;
	hashtable = std::vector<hash_entry_t>(HASHTABLE_MASK + 1);

	// prime hash table
	uint32_t* data = (uint32_t*)(nodes + tree_size - 1);
	while ((uint8_t*)data < ((uint8_t*)root) + filesize)
	{
		node_t* node = from_index(*data++);
		if (!node)
			node = root;
		uint32_t size = *data++;
		if (!lookup_subtree_size(node))
		{
			bool success = save_subtree_size(node, size);
			assert(success);
		}
	}

	return true;
}

void WatkinsTree::close_tree()
{
	tree_size = 0;
#ifdef _WIN32
	UnmapViewOfFile(root);
	CloseHandle((HANDLE)mapping);
#else
	munmap(root, num_pages * sysconf(_SC_PAGE_SIZE));
	close(fd);
#endif
}

bool WatkinsTree::is_open() const
{
	return tree_size != 0;
}


std::vector<SolutionEntry> WatkinsTree::get_solution(const std::vector<uint16_t>& moves, bool calc_num_nodes)
{
	std::vector<SolutionEntry> entries;

	query_result_t result;
	const node_t* node = root;
	for (size_t i = 0; i < moves.size(); i++)
	{
		move_t move = pgMove_to_wMove(moves[i]);
		node = get_move(move, node);
		if (!node)
			return entries;
	}
	if (!node)
		return entries;

	bool is_ok = calc_num_nodes ? query_children(node, &result): query_children_fast(node, &result);
	if (!is_ok)
		return entries;

	if (result.size() == 0)
		return entries;

	if (calc_num_nodes)
		std::sort(result.begin(), result.end(), 
			[](const move_branch_t& a, const move_branch_t& b) { 
				return a.size > b.size;
			}
		);

	if (!result.front().move)
		return entries;

	for (auto& r : result)
	{
		uint32_t num_nodes = calc_num_nodes ? r.size : 1;
		uint16_t pgMove = wMove_to_pgMove(r.move);
		entries.emplace_back(pgMove, ESOLUTION_VALUE, num_nodes);
	}
	return entries;
}

std::shared_ptr<std::vector<uint16_t>> WatkinsTree::opening_moves(const std::filesystem::path& filepath)
{
	using namespace std;
	error_code err;
	auto filesize = fs::file_size(filepath, err);
	if (filesize < 6)
		return nullptr;

	ifstream file(filepath, ios::binary | ios::in);
	node_t node;
	file.read(reinterpret_cast<char*>(&node), 6);
	if (node.move > MAX_NUM_OPENING_MOVES)
		return nullptr;
	if (sizeof(node) + node.move * sizeof(move_t) > filesize)
		return nullptr;

	auto moves = make_shared<vector<uint16_t>>(node.move);
	file.read(reinterpret_cast<char*>(moves->data()), node.move * sizeof(move_t));
	for (uint16_t& move : *moves)
		move = wMove_to_pgMove(move);
	return moves;
}

std::shared_ptr<std::vector<uint16_t>> WatkinsTree::opening_moves()
{
	using namespace std;
	if (!is_open() || prolog_len > MAX_NUM_OPENING_MOVES)
		return nullptr;

	auto moves = make_shared<vector<uint16_t>>(prolog_len);
	move_t* pmove = prolog;
	for (uint16_t& move : *moves)
		move = wMove_to_pgMove(*pmove++);
	return moves;
}
