#include "solverresults.h"
#include "solutionbook.h"

#include <fstream>

using namespace std;


SolverResults::SolverResults(std::shared_ptr<Solution> solution)
	: Solver(solution)
{}

void SolverResults::merge_books(std::list<QString> books, const std::string& file_to_save)
{
    // First books are prioritized.
	// Assuming each book has one entry/move for each key.
	// Duplicates are saved, except for null-move duplicates.
	
    size_t num_duplicates = 0;
	size_t num_null_duplicates = 0;
	size_t num_elements = 0;

    /// Load
	map<uint64_t, list<uint64_t>> all_entries;
    for (auto& path_i : books)
	{
		//auto path_i = path(type, FileSubtype::Std);
		map<uint64_t, uint64_t> entries = read_book(path_i.toStdString());
		for (auto& [key, val] : entries)
		{
			auto it = all_entries.find(key);
			if (it == all_entries.end())
			{
				all_entries[key] = {val};
			}
			else
			{
				uint64_t move = val >> 48;
				if (move) {
					uint64_t prev_move = it->second.back() >> 48;
					if (!prev_move)
					{
						it->second.pop_back();
						num_null_duplicates++;
					}
					it->second.push_back(val);
				}
				else
				{
					num_null_duplicates++;
				}
				num_duplicates++;
			}
			num_elements++;
		}
    }

    /// Check duplicates
	if (num_duplicates)
		emit_message(QString("Merge books: Found %1 duplicate%2, including %3 null-move%4")
		                 .arg(num_duplicates)
		                 .arg(num_duplicates == 1 ? "" : "s")
		                 .arg(num_null_duplicates)
		                 .arg(num_null_duplicates == 1 ? "" : "s"));
	else
		emit_message("Merge books: No duplicates");
	
    /// Save
	uintmax_t filesize = num_elements * 16;
	vector<char> data_to_save(filesize);
	size_t n = 0;
	for (auto& [key, values] : all_entries) {
		for (auto& val : values) {
			save_bigendian(key, data_to_save.data() + n);
			save_bigendian(val, data_to_save.data() + n + 8);
			n += 16;
		}
	}
	ofstream file(file_to_save, ios::binary | ios::out | ios::trunc);
	file.write((const char*)data_to_save.data(), filesize);
    emit_message(QString("Merge books: %1 merged").arg(sol->nameToShow(true)));
}
