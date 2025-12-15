#ifndef SOLUTION_H
#define SOLUTION_H

#include "solutionbook.h"
#include "positioninfo.h"
#include "side.h"
#include "board/move.h"
#include "board/board.h"
#include "watkins/watkinssolution.h"

#include <QStringList>
#include <QChar>
#include <QSettings>

#include <array>
#include <list>
#include <vector>
#include <map>
#include <memory>
#include <tuple>
#include <functional>


struct LIB_EXPORT BranchToSkip
{
	BranchToSkip(const Line& branch, int score)
		: branch(branch), score(score)
	{}
	Line branch;
	int score;
};


struct LIB_EXPORT SolutionData
{
	SolutionData()
		: WatkinsStartingPly(-1)
	{}
	SolutionData(const Line& opening, const Line& branch, const QString& tag, const std::list<BranchToSkip>& branchesToSkip, 
		const QString& Watkins, int WatkinsStartingPly, const QString& folder)
	    : opening(opening), branch(branch), tag(tag), branchesToSkip(branchesToSkip), Watkins(Watkins), WatkinsStartingPly(WatkinsStartingPly), folder(folder)
	{}
	Line opening;
	Line branch;
	QString tag;
	std::list<BranchToSkip> branchesToSkip;
	QString Watkins;
	int WatkinsStartingPly;
	QString folder;
};


class Solution;
using SolutionCollection = std::map<QString, std::list<std::shared_ptr<Solution>>>;


class LIB_EXPORT Solution : public QObject
{
	Q_OBJECT

public:
	enum class FileSubtype
	{
		Std, New, Bak
	};

	constexpr static int SOLUTION_VERSION = 1;
	
public:
	Solution(std::shared_ptr<SolutionData> data, const QString& name = "", int version = SOLUTION_VERSION, bool is_imported = false);

	static std::shared_ptr<Solution> load(const QString& filepath);
	static std::tuple<SolutionCollection, QString> loadFolder(const QString& folder_path);
	static QString ext_to_bak(const QString& filepath);

	bool isValid() const;
	bool isBookOpen() const;
	Chess::Side winSide() const;
	Line openingMoves(bool with_branch = false) const;
	std::list<MoveEntry> entries(Chess::Board* board) const;
	std::list<MoveEntry> nextEntries(Chess::Board* board, std::list<MoveEntry>* missing_entries = nullptr) const;
	std::list<MoveEntry> positionEntries(Chess::Board* board, bool use_esolution = true);
	std::list<MoveEntry> nextPositionEntries(Chess::Board* board, std::list<MoveEntry>* missing_entries = nullptr);
	std::list<MoveEntry> esolEntries(Chess::Board* board, std::list<MoveEntry>* missing_entries = nullptr);
	std::shared_ptr<SolutionEntry> bookEntry(std::shared_ptr<Chess::Board> board, FileType type, bool check_cache = true) const;
	std::shared_ptr<SolutionEntry> bookEntry(Chess::Board* board, FileType type, bool check_cache = true) const;
	QString positionInfo(std::shared_ptr<Chess::Board> board);
	QString eSolutionInfo(Chess::Board* board);
	QString info() const;
	QString nameToShow(bool include_tag = false) const;
	std::tuple<QString, QString> branchToShow(bool include_tag = false) const;
	const QString& nameString() const;
	QString score() const;
	QString nodes() const;
	const QString& subTag() const;
	bool exists() const;
	bool hasWatkinsSolution() const;
	std::shared_ptr<SolutionData> mainData() const;

	void initFilenames();
	void loadBook(bool ignore_lower_level = false);
	void updateInfo();
	void activate(bool send_msg = true);
	void deactivate(bool send_msg = true);
	bool remove(std::function<bool(const QString&)> are_you_sure, std::function<void(const QString&)> message);
	void edit(std::shared_ptr<SolutionData> data);
	bool mergeAllFiles();
	void addToBook(std::shared_ptr<Chess::Board> board, const SolutionEntry& entry, FileType type);
	void addToBook(quint64 key, const SolutionEntry& entry, FileType type);
	void addToBook(std::shared_ptr<Chess::Board> board, uint64_t data, FileType type);
	std::vector<SolutionEntry> eSolutionEntries(std::shared_ptr<Chess::Board> board, bool use_cache = true);
	std::vector<SolutionEntry> eSolutionEntries(Chess::Board* board, bool use_cache = true);
	QString bookFolder() const;
	QString path(FileType type, FileSubtype subtype = FileSubtype::Std) const;
	bool fileExists(FileType type, FileSubtype subtype = FileSubtype::Std) const;

signals:
	void Message(const QString&, MessageType type = MessageType::std);

private:
	int winInValue(std::shared_ptr<Chess::Board> board, FileType type) const;
	bool hasMergeErrors() const;
	void saveBranchSettings(QSettings& s, std::shared_ptr<Chess::Board> board);
	bool mergeFiles(FileType type) const;
	void addToBook(const EntryRow& row, FileType type) const;

private:
	Line opening;
	Line branch;
	QString tag;
	std::list<BranchToSkip> branchesToSkip;
	QString Watkins;
	int WatkinsStartingPly;
	std::vector<Chess::Move> WatkinsOpening;
	QString WatkinsOpeningSan;
	WatkinsTree WatkinsSolution;
	QString folder;
	QString name;
	Chess::Side side;
	int version;
	bool is_imported;
	int info_win_in;

	std::shared_ptr<SolutionBook> book_main;
	std::array<QString, FileType_SIZE> filenames;
	std::array<QString, FileType_DATA_END> filenames_new;
	std::array<std::shared_ptr<SolutionBook>, FileType_DATA_END> books;
	std::array<std::map<uint64_t, SolutionEntry>, FileType_DATA_END> data_new;
	int64_t ram_budget;

	friend class Solver;
	friend class SolverResults;
	bool is_solver_upper_level;

public:
	static const QString DATA;
	static const QString BOOKS;
	static const QString SPEC_EXT;
	static const QString DATA_EXT;
	static const QString TDATA_EXT;
	static const QString BAK_EXT;
};

#endif // SOLUTION_H
