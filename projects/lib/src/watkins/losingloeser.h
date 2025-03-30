#ifndef LOSINGLOESER_H_
#define LOSINGLOESER_H_

#include "positioninfo.h"

#include <QObject>
#include <QString>

#include <memory>
#include <map>
#include <atomic>
#include <chrono>

namespace Chess
{
	class Board;
}


struct MoveResult
{
	QString depth;
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
		stopping
	};

public:
	LosingLoeser();

public:
	size_t requiredRAM(int Mnodes) const;
	bool isBusy() const;
	void start(Chess::Board* ref_board, uint32_t total_nodes, size_t max_moves);
	void stop();

private:
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

private:
	constexpr static uint32_t step_nodes = 200000;
	constexpr static uint32_t cache_size = 500;
};

#endif // LOSINGLOESER_H_
