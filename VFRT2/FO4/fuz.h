#include <fstream>
#include <vector>
#include <string>


extern std::wstring xenc;

std::string fuz_extract(const std::string &buffer, std::wstring destdir, std::string fname, bool cvt=true, bool lip=false);