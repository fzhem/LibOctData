#pragma once

#include <string>
#include<filesystem>

#include <boost/predef.h>
#include <codecvt>

inline std::string convertUTF16StringToUTF8(const std::u16string& u16)
{
#if BOOST_COMP_MSVC == false
	static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
	return converter.to_bytes(u16);
#else
	static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
	std::wstring wstr(u16.begin(), u16.end());
	return convert.to_bytes(wstr);
#endif
}
