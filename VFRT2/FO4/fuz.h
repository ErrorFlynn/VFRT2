#include <fstream>
#include <vector>
#include <string>

using namespace std;

extern wstring xenc;

string fuz_extract(const string &buffer, wstring destdir, string fname, bool cvt=true, bool lip=false);
//unsigned long RunPipedProcess(wstring cmdline, string &output, bool force_output);