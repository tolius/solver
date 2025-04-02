#ifndef LOSINGLOESER_H_
#define LOSINGLOESER_H_

#include "positioninfo.h"

#include "board/move.h"

#include <QObject>
#include <QString>

#include <memory>
#include <map>
#include <tuple>
#include <vector>
#include <atomic>
#include <chrono>

namespace Chess
{
	class Board;
}


struct MoveResult
{
	uint32_t size;
	uint16_t pgMove;
	int16_t value;
	QString eval;
	QString san;
};

struct LLdata
{
	int64_t time_us;
	uint32_t size;
	float speed_ll_Mns;
	float speed_egtb_kns;
	QString best_eval;
	QString best_move;
	QString main_line;
	int progress;
	std::vector<MoveResult> moves;

	std::chrono::steady_clock::time_point time_point;
	uint32_t total_nodes;

	bool empty() const;
};

Q_DECLARE_METATYPE(LLdata)
Q_DECLARE_METATYPE(MessageType)


class LosingLoeser : public QObject
{
	Q_OBJECT

	enum class Status : int
	{
		idle,
		initialized,
		calculating,
		stopping,
		processing
	};

private:
	LosingLoeser();

	static std::shared_ptr<LosingLoeser> _instance;

public:
	static std::shared_ptr<LosingLoeser> instance();

public:
	size_t requiredRAM(int Mnodes) const;
	bool isBusy() const;
	void start(Chess::Board* ref_board, uint32_t total_nodes, size_t max_moves);
	void stop();
	std::vector<MoveResult> get_results(Chess::Board* ref_board);
	std::shared_ptr<Chess::Board> get_results_board();
	void clear();

private:
	std::tuple<Chess::Move, QString, bool> get_wMove_info(move_t wMove);
	std::tuple<Chess::Move, QString, bool> get_pgMove_info(std::shared_ptr<Chess::Board> pos, move_t pgMove);
	std::vector<move_t> setPosition(Chess::Board* ref_board);
	void run();

public slots:
	void Run();

signals:
	void Message(const QString& msg, MessageType type = MessageType::std);
	void Progress(const LLdata& data);
	void Finished();

private:
	std::shared_ptr<Chess::Board> board;
	size_t max_moves;
	uint32_t total_nodes;
	std::atomic<Status> status;
	std::map<quint64, LLdata> res_cache;
	uint64_t curr_result_key;

private:
	constexpr static uint32_t step_nodes = 200000;
	constexpr static uint32_t cache_size = 500;
};

#endif // LOSINGLOESER_H_
