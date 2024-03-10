#ifndef WATKINSSOLUTION_H_
#define WATKINSSOLUTION_H_

#include "solutionbook.h"

#include <cstdint>
#include <vector>
#include <list>
#include <tuple>
#include <memory>
#include <filesystem>


using move_t = uint16_t;

#pragma pack(push, 1)
struct node_t
{
	uint32_t data;
	uint16_t move;
};
#pragma pack(pop)
static_assert(sizeof(node_t) == 6, "node_t packed");

struct hash_entry_t
{
	uint32_t index;
	uint32_t size;
};

struct move_branch_t
{
	move_branch_t(move_t move, uint32_t size)
		: move(move), size(size)
	{}

	move_t move;
	uint32_t size;
};

using query_result_t = std::vector<move_branch_t>;


class WatkinsTree
{
public:
	WatkinsTree();
	~WatkinsTree();

	bool open_tree(const std::filesystem::path& filepath);
	void close_tree();
    bool is_open() const;
	std::vector<SolutionEntry> get_solution(const std::vector<uint16_t>& moves, bool calc_num_nodes);

private:
	const node_t* next(const node_t* node) const;
	node_t* from_index(uint32_t index) const;
	uint32_t node_index(const node_t* node) const;
	const node_t* trans(const node_t* node) const;
	const node_t* trans_ns(const node_t* node) const;
	const node_t* next_sibling(const node_t* node) const;
	uint32_t lookup_subtree_size(const node_t* node) const;
	const node_t* get_move(move_t move, const node_t* node) const;

	bool save_subtree_size(const node_t* node, uint32_t size);
	void walk(const node_t* node, bool transpositions);
	uint32_t get_subtree_size(const node_t* node);

	bool query_children(const node_t* node, query_result_t* result);
	bool query_children_fast(const node_t* node, query_result_t* result);
	bool query(const move_t* moves, size_t moves_len, query_result_t* result);

private:
	int fd;
#ifdef _WIN32
	void* mapping = nullptr;
#else
	size_t num_pages;
#endif

	uint32_t prolog_len;
	move_t* prolog;

	node_t* root;
	uint32_t tree_size = 0;

	node_t* nodes;

	std::vector<hash_entry_t> hashtable;
	size_t num_hash_entries;

	std::vector<uint64_t> arr;
};

#endif  // #ifndef WATKINSSOLUTION_H_
