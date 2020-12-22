#include <string>
#include <filesystem>
#include "dbwrapper.h"
namespace fs = std::filesystem;

void ShowUsage(const std::string& name)
{
    std::cerr << "Usage: " << name << " db_path output_file_path\n"
	      << "db_path is the path to the chainstate folder \n"
		  << "output_file_path is the path to the file that will be created by the app with all balances " << std::endl;
}

int main(int argc, char* argv[])
{
	if (argc < 3) {
		ShowUsage(argv[0]);
		return EXIT_FAILURE;
	}

	fs::path dbPath = argv[1];
	fs::path outputPath = argv[2];

	try {
		DBWrapper db(dbPath);
		db.dumpAllUTXOs(outputPath);
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
