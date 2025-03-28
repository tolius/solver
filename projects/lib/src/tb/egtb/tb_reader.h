#ifndef _TB_READER_H_
#define _TB_READER_H_

#include "../position.h"
#include "tb_idx.h"

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <mutex>
#include <filesystem>

#ifdef USE_FAIRY_SF
using namespace Stockfish;
#endif
namespace fs = std::filesystem;

namespace egtb
{
	constexpr bool     DTZ101_AS_DRAW  = true;
	constexpr bool     ALWAYS_SAVE_DTZ = false;
	constexpr bool     DO_EP_POSITIONS = true;
	constexpr size_t   MAX_IDX_SIZE    = 28;

	constexpr uint16_t EGTB_IS_COMPRESSED     = 1 << 0;
	constexpr uint16_t EGTB_HAS_DTZ           = 1 << 1;
	constexpr uint16_t EGTB_VAL_BIG           = 1 << 2;
	constexpr uint16_t EGTB_VAL_12BIT         = 1 << 3;
	constexpr uint16_t EGTB_DTZ101_AS_DRAW    = 1 << 4;
	//...
	constexpr uint16_t EGTB_SKIP_ONLY_MOVES   = 1 << 6; // reserved
	constexpr uint16_t EGTB_VAL_DTZ_SEPARATED = 1 << 7; // reserved

	constexpr int16_t  DRAW  = 0x3333;
	constexpr int16_t  NONE  = 0x5555;
	constexpr int16_t  EMPTY = 0x7777;

	constexpr int16_t  B12_DRAW  = 0x07DD;
	constexpr int16_t  B12_EMPTY = 0x07EE;
	constexpr int16_t  B12_NONE  = 0x07FF;

	constexpr uint8_t  DTZ_NO_WIN  = 0xFF;
	constexpr uint8_t  DTZ_NO_LOSS = 0xFE;
	constexpr uint8_t  DTZ_NONE    = 0xFD;
	constexpr uint8_t  DTZ_MAX     = 101;

	using DZ_Header = void*;
	using val_dtz = std::tuple<int16_t, uint8_t>;

	class TB_Reader
	{
	public:
		static fs::path egtb_path;
		static std::string file_extension_compressed;
		static std::map<std::string, std::shared_ptr<TB_Reader>> tb_cache;
	protected:
		static bool auto_load;

	public:
		static void init(const std::string& tb_path, bool load_all = false);
		static bool probe_EGTB(Position& board, int16_t& val, uint8_t& dtz);
		static void print_EGTB_info();
		static void discard_tb(const std::string& tb_name);

	public:
		TB_Reader(const std::string& tb_name);
		~TB_Reader();

	protected:
		void init_idx(const std::vector<char>& idx_data);
		bool is_dz_open() const;
		uint16_t read_flags(DZ_Header header, bool is_compressed) const;
		void error(const std::string& text, bool throw_exception = true) const;

		std::tuple<size_t, bool> board_to_index(const Position& board) const;
		size_t board_to_key(const Position& board, bool is_ep = false) const;
		void read_val_12bit(char* p_bytes, size_t key, const char* p_data) const;
		val_dtz read_one(size_t key, bool is_compressed = true);
		val_dtz load_one(const char* tb_bytes, bool is_compressed, bool val_big) const;

		val_dtz probe_one(Position& board);
		std::tuple<bool, int16_t, uint8_t> probe_ep(Position& board);
		static std::tuple<bool, int16_t, uint8_t> probe_tb(Position& board);

	protected:
		std::string tb_name;
		bool has_dtz;
		bool has_pawns;
		bool is_symmetrical;
		
		DZ_Header dz_header;
		uint16_t tb_flags;
		std::mutex mtx_dz;
		
		std::shared_ptr<TBTable> tbtable;
		size_t size;
		size_t size2;
	};

} // namespace egtb
#endif
