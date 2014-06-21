// Copyright (c) 2014, 임경현 (dlarudgus20)
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <ctype.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <iterator>
#include <string>
#include <unordered_map>
#include <utility>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;
namespace balgo = boost::algorithm;

typedef std::unordered_map<std::wstring, std::wstring>::iterator script_iterator;

std::unordered_map<std::wstring, std::wstring> script_map_byname = {
	{ L"makefile", L"# [[string]]\n" },
	{ L"app.config", L"<!--\n [[string]]\n-->\n" },
};

std::unordered_map<std::wstring, std::wstring> script_map_byext = {
	{ L".c", L"/*\n * [[string]]\n */\n" },
	{ L".cpp", L"// [[string]]\n" },
	{ L".s", L"// [[string]]\n" },
	{ L".asm", L"; [[string]]\n" },
	{ L".cs", L"// [[string]]\n" },
	{ L".vb", L"' [[string]]\n" },
	{ L".xml", L"<!--\n [[string]]\n-->\n" },
	{ L".xaml", L"<!--\n [[string]]\n-->\n" },
	{ L".html", L"<!--\n [[string]]\n-->\n" },
	{ L".settings", L"<!--\n [[string]]\n-->\n" },
	{ L".txt", L"!! [[string]]\n" },
};

const std::wstring head_notice =
	L"`` license notice is written by AutoLicense (by 임경현)"
	L" <https://github.com/dlarudgus20/AutoLicense> ``\n";

void process(const bfs::wpath &path, const std::wstring &script, const std::wstring &license,
	const bfs::wpath &tmp_path);

std::wifstream open_input_file(const std::wstring &fname, bool *is_empty = nullptr);
std::wifstream open_input_file(const std::string &fname, bool *is_empty = nullptr);
std::wifstream open_input_file(std::wifstream f, bool *is_empty = nullptr);

std::wofstream open_output_file(const std::wstring &fname);
std::wofstream open_output_file(const std::string &fname);
std::wofstream open_output_file(std::wofstream f);

bool assume_default_is_ansi;
bool quiet_processing;

int main(int argc, char *argv[])
{
	std::string dir;
	std::string license_file;

	bpo::options_description des("사용 가능한 옵션들");
	des.add_options()
		("help,h", "도움말을 출력합니다.")
		("license-file,f", bpo::value<std::string>(&license_file)->default_value("LICENSE"),
			"set license file")
		("dir,d", bpo::value<std::string>(&dir)->default_value("."),
			"set directory")
		("quiet,q", bpo::bool_switch(&quiet_processing)->default_value(false),
			"quiet processing [default is false]")
		("assume-default-is-ansi", bpo::bool_switch(&assume_default_is_ansi)->default_value(false),
			"assume default encoding (aka, no BOM) is ansi [default is false]")
	;

	bpo::variables_map vm;

	try
	{
		bpo::store(bpo::parse_command_line(argc, argv, des), vm);
		bpo::notify(vm);
	}
	catch (bpo::error &e)
	{
		std::cerr << e.what() << "\n\n" << des << '\n';
		return -1;
	}

	if (vm.count("help"))
	{
		std::cout << des << '\n';
		return 0;
	}
	else
	{
		if (assume_default_is_ansi)
		{
			std::cerr << "'--assume-default-is-ansi' is not supported yet :)\n";
			return -1;
		}

		std::wstring license(head_notice);

		std::copy(
			std::istreambuf_iterator<wchar_t> { open_input_file(license_file) },
			std::istreambuf_iterator<wchar_t> { },
			std::back_inserter(license));

		balgo::trim(license);

		char tmp_path_str[L_tmpnam];
		if (tmpnam(tmp_path_str) == NULL)
			throw std::runtime_error("creating temporary file failed");
		bfs::wpath tmp_path(tmp_path_str);

		bfs::wrecursive_directory_iterator dir_end;
		for (bfs::wrecursive_directory_iterator dir_it(dir); dir_it != dir_end; ++dir_it)
		{
			if (bfs::is_regular_file(dir_it->status()))
			{
				script_iterator script_it;
				auto path = dir_it->path();

				std::wstring filename = path.filename().wstring();
				balgo::to_lower(filename);

				std::wstring ext = path.extension().wstring();
				balgo::to_lower(ext);

				if ((script_it = script_map_byname.find(filename)) != script_map_byname.end())
				{
					process(path, script_it->second, license, tmp_path);
				}
				else if ((script_it = script_map_byext.find(ext)) != script_map_byext.end())
				{
					process(path, script_it->second, license, tmp_path);
				}
			}
		}

		bfs::remove(tmp_path);
	}
}

void process(const bfs::wpath &path, const std::wstring &script, const std::wstring &license,
	const bfs::wpath &tmp_path)
{
	std::wstring line;
	std::wstring::size_type idx;
	bool is_empty;

	if (!quiet_processing)
		std::wcout << path << '\n';

	std::wifstream target_file = open_input_file(path.string(), &is_empty);
	if (!is_empty)
	{
		std::getline(target_file, line);
		balgo::to_lower(line);
		if (line.find(L"@noautolicense") != std::wstring::npos)
			return;
	}
	target_file.close();

	std::wofstream tmp_file = open_output_file(tmp_path.string());

	std::wstringstream strm(script);
	std::wstringstream license_strm(license);

	bool bLicenseProced = false;

	while (!strm.eof())
	{
		std::getline(strm, line);

		idx = line.find(L"[[string]]");
		if (!bLicenseProced && idx != std::wstring::npos)
		{
			bLicenseProced = true;

			std::wstring prefix = idx != 0 ? line.substr(0, idx) : L"";
			idx += 10;
			std::wstring postfix = line.substr(idx);

			std::wstring license_line;

			while (!license_strm.eof())
			{
				std::getline(license_strm, license_line);
				tmp_file << prefix << license_line << postfix << L'\n';
			}
		}
		else
		{
			tmp_file << line << L'\n';
		}
	}

	target_file = open_input_file(path.string(), &is_empty);
	if (!is_empty)
	{
		tmp_file << open_input_file(path.string()).rdbuf();
	}

	tmp_file.flush();
	tmp_file.close();

	// copy temp file to target file
	std::wofstream(path.string(), std::ios::binary)
		<< std::wifstream(tmp_path.string(), std::ios::binary).rdbuf();
}

std::wifstream open_input_file(const std::wstring &fname, bool *is_empty /* = nullptr*/)
{
	return open_input_file(std::wifstream { fname, std::ios::in | std::ios::binary }, is_empty);
}
std::wifstream open_input_file(const std::string &fname, bool *is_empty /* = nullptr*/)
{
	return open_input_file(std::wifstream { fname, std::ios::in | std::ios::binary }, is_empty);
}
std::wifstream open_input_file(std::wifstream f, bool *is_empty /* = nullptr*/)
{
	bool tmp;
	if (is_empty == nullptr)
		is_empty = &tmp;	// trick for no-checking null

	f.exceptions(std::ios::failbit | std::ios::badbit);

	if (f.peek() == std::wifstream::traits_type::eof())
	{
		*is_empty = true;
		f.clear();
		goto AssumeFormat;
	}
	else
	{
		*is_empty = false;
	}

	wchar_t ch = f.get();
	if (ch == L'\xef')
	{
		ch = f.get();
		wchar_t ch2 = f.get();
		if (ch != L'\xbb' || ch2 != L'\xbf')
			throw std::runtime_error("encoding error");

		f.imbue(
			std::locale(f.getloc(), new std::codecvt_utf8<wchar_t>())
			);
	}
	else
	{
		if (ch == L'\xfe' || ch == L'\xff')
			throw std::runtime_error("utf-8/16 is not supported");

	AssumeFormat:
		f.seekg(0, std::ios::beg);

		//if (assume_default_is_ansi)
		//{
		//	// assume Ansi
		//	// it's not implemented yet, so ignore option and run UTF-8
		//}
		//else
		{
			// assume UTF-8
			f.imbue(
				std::locale(f.getloc(), new std::codecvt_utf8<wchar_t>())
				);
		}
	}
	return f;
}

std::wofstream open_output_file(const std::wstring &fname)
{
	return open_output_file(std::wofstream { fname,
		std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary });
}
std::wofstream open_output_file(const std::string &fname)
{
	return open_output_file(std::wofstream { fname,
		std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary });
}
std::wofstream open_output_file(std::wofstream f)
{
	f.exceptions(std::ios::failbit | std::ios::badbit);
	f.write(L"\xef\xbb\xbf", 3);

	f.imbue(
		std::locale(f.getloc(), new std::codecvt_utf8<wchar_t>())
		);
	return f;
}
