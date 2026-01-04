




#include "main.h"

#include "bitboard.h"
#include "movegen.h"
#include "debug.h"
#include "simple_search.h"
#include "atomicdata.h"
#include "search.h"
#include "bitboard.h"
#include "thread.h"
#include "ucioption.h"
#include "book.h"
// #include "fics.h"
#include "create_book.h"
#include "pgn.h"
#include "evaluate.h"
#include "nnue.h"
#include "datagen/datagen.h"

#include <pthread.h>
#include <queue>
#include <iostream>
#include <sstream>
#include <string>
#include <time.h>
// #include <conio.h>
using namespace std;

#if defined ANALYZE_VERSION
	extern void main_analyze();
#elif defined BOOK_VERSION
	extern void main_book_from_thinking();
	extern void main_book_from_file();
#elif defined FICS_VERSION
	extern void main_fics();
#elif defined UCI_VERSION
	extern void main_uci(int argc);
#endif




extern void init_kpk_bitbase();

//#define TEST1
#define TEST2

int main(int argc, char* argv[]) {
	srand(time(0));
	
	
	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	cout.rdbuf()->pubsetbuf(NULL, 0);
	cin.rdbuf()->pubsetbuf(NULL, 0);
	
	// Startup initializations
	init_bitboards();
	Position::init_zobrist();
	Position::init_piece_square_tables();
	init_kpk_bitbase();
	init_search();
	Threads.init();
	generate_explosionSquares();
	generate_squaresTouch();

	if (!nnue::load(Options["EvalFile"].value<string>())) {
		const string& err = nnue::last_error();
		if (!err.empty())
			cout << "NNUE: " << err << endl;
	}
	
	
#ifndef NDEBUG
	cout << "Debug version of atomkraft, define NDEBUG to get the release version." << endl;
#endif
	
	// Print copyright notice
	cout << engine_name() << endl;
	cout << "by " << engine_authors() << endl;
	cout << "built " << __DATE__ << " " << __TIME__ << endl << endl;
	
	if (CpuHasPOPCNT)
		cout << "Good! CPU has hardware POPCNT." << endl;
	
	
#ifdef SWEN_VERSION
	Options["Threads"] = UCIOption(2, 1, MAX_THREADS);
	Options["Hash"] = UCIOption(512, 4, 8192);
	Options["Book File"] = UCIOption("eao.bin");
#endif
	
	
	
	
#if defined ANALYZE_VERSION
    
    main_analyze();

#elif defined BOOK_VERSION
	
	main_book_from_file();
	//main_book_from_thinking();
	
#elif defined FICS_VERSION
	
	main_fics();


#elif defined UCI_VERSION

	// Check for datagen command
	if (argc >= 2 && string(argv[1]) == "datagen") {
		// Usage: ATOMKRAFT64.exe datagen <nnue_file> <output_dir> [threads] [games_per_thread]
		if (argc < 4) {
			cout << "Usage: " << argv[0] << " datagen <nnue_file> <output_dir> [threads=1] [games_per_thread=0]" << endl;
			cout << "  nnue_file: Path to NNUE file for evaluation (e.g., atomic-07a.nnue)" << endl;
			cout << "  output_dir: Directory to write .bin files" << endl;
			cout << "  threads: Number of parallel threads (default: 1)" << endl;
			cout << "  games_per_thread: Games per thread, 0=infinite (default: 0)" << endl;
			cout << endl;
			cout << "Example: " << argv[0] << " datagen atomic-07a.nnue datagen_output 4 1000" << endl;
			return 1;
		}

		string nnueFile = argv[2];
		string outputDir = argv[3];
		int threads = (argc >= 5) ? atoi(argv[4]) : 1;
		int gamesPerThread = (argc >= 6) ? atoi(argv[5]) : 0;

		datagen::run(nnueFile, outputDir, threads, gamesPerThread);
	} else {
		main_uci(argc);
	}

#else
	
	cout << "no version specified" << endl;
	
#endif
	
	
	
	
	return 0;
}









