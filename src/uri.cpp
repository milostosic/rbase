//--------------------------------------------------------------------------//
/// Copyright (c) 2018 by Milos Tosic. All Rights Reserved.                ///
/// License: http://www.opensource.org/licenses/BSD-2-Clause               ///
//--------------------------------------------------------------------------//

#include <rbase_pch.h>
#include <rbase/inc/uri.h>

namespace rtm {

bool isPartValid(const StringView& _view, UriPart::Enum _part)
{
	switch (_part)
	{
		case UriPart::Scheme:
			{
				const uint32_t len = _view.length();
				for (uint32_t i = 0; i < len; ++i)
				{
					char c = _view[i];
					if (!(false
						|| isAlphaNum(c)
						|| (c == '+')
						|| (c == '.')
						|| (c == '-')))
						return false;
				}
				return true;
			}
			break;

		default:
			return false;
	};
}

UriView::UriView()
{
}

UriView::UriView(const char* _str, uint32_t _len)
{
	parse(StringView(_str, _len));
}

UriView::UriView(const StringView& _str)
{
	parse(_str);
}

void UriView::clear()
{
	for (int i=0; i<UriPart::Count; ++i)
		m_parts[i].clear();
}

void UriView::parse(const StringView& _str)
{
	const char* schemeEnd = strStr(_str, ":");
	if (!schemeEnd)
		return clear();

	m_parts[UriPart::Scheme] = StringView(_str.data(), schemeEnd);
	if (!isPartValid(m_parts[UriPart::Scheme], UriPart::Scheme))
		return clear();

	const char* authority		= strStr(_str, "//");
	const char* authorityEnd	= 0;
	const char* strEnd			= _str.data() + _str.length();

	if (!authority)
		authority = schemeEnd + 1;
	else
		authority += 2;

	const char* userInfoEnd = strStr(authority, "@");
	authorityEnd = strStr(authority, "/");

	if (!authorityEnd)
		authorityEnd = strEnd;

	m_parts[UriPart::Authority] = StringView(authority, authorityEnd);

	const char* hostStart = authority;

	if (userInfoEnd)
	{
		hostStart = userInfoEnd + 1;

		StringView user = StringView(authority, userInfoEnd);
		m_parts[UriPart::User] = user;
		if (const char* pwd = strStr(user, ":"))
		{
			m_parts[UriPart::UserName] = StringView(authority, pwd);
			m_parts[UriPart::Password] = StringView(pwd + 1, userInfoEnd);
		}
	}

	StringView hostAndPort(hostStart, authorityEnd);
	if (const char* port = strStr(hostAndPort, ":"))
	{
		m_parts[UriPart::Host] = StringView(hostStart, port);
		m_parts[UriPart::Port] = StringView(port + 1, authorityEnd);
	}
	else
		m_parts[UriPart::Host] = StringView(hostStart, authorityEnd);

	if (*authorityEnd == '/')
	{
		const char* path		= strStr(authorityEnd, "/");
		const char* query		= strStr(authorityEnd, "?");
		const char* fragment	= strStr(authorityEnd, "#");

		const char* pathEnd		= query < fragment ? query : fragment;
		if (!pathEnd)
			pathEnd = strEnd;

		m_parts[UriPart::Path]	= StringView(path, pathEnd);

		if (query)
		{
			if (fragment)
				m_parts[UriPart::Query] = StringView(query + 1, fragment);
			else
				m_parts[UriPart::Query] = StringView(query + 1, strEnd);
		}

		if (fragment)
			m_parts[UriPart::Fragment]	= StringView(fragment + 1, strEnd);
	}
}

const StringView& UriView::get(UriPart::Enum _part) const
{
	return m_parts[_part];
}

static inline int shouldEncode(char ch)
{
	return !(false
		|| isAlphaNum(ch)
		|| '-' == ch || '_' == ch
		|| '.' == ch || '!' == ch
		|| '~' == ch || '*' == ch
		|| '(' == ch || ')' == ch);
}

uint32_t uriEncode(const StringView& _str, char* _buffer, uint32_t _bufferSize)
{
	return uriEncode(_str.data(), _buffer, _bufferSize, _str.length());
}

uint32_t uriEncode(const char* _uri, char* _buffer, uint32_t _bufferSize, uint32_t _maxUriChars)
{
	RTM_ASSERT(_uri, "");
	RTM_ASSERT(_buffer, "");

	uint32_t len = _maxUriChars == UINT32_MAX ? strLen(_uri) : _maxUriChars;
	const char* uriEnd = _uri + len;

	uint32_t dSize = 0;
	while (_uri < uriEnd)
	{
		if (dSize >= _bufferSize)
		{
			_buffer[0] = '\0';
			return UINT32_MAX;
		}

		char ch = *_uri++;

		if (shouldEncode(ch))
		{
			if (dSize < _bufferSize - 3)
			{
				_buffer[dSize++] = '%';
				_buffer[dSize++] = toHexNum(ch >> 4);
				_buffer[dSize++] = toHexNum(ch);
			}
			else
			{
				_buffer[0] = '\0';
				return UINT32_MAX;
			}
		}
		else
			_buffer[dSize++] = ch;
	}

	_buffer[dSize++] = '\0';

	return dSize;
}

uint32_t uriDecode(const StringView& _str, char* _buffer, uint32_t _bufferSize)
{
	return uriDecode(_str.data(), _buffer, _bufferSize, _str.length());
}

uint32_t uriDecode(const char* _uri, char* _buffer, uint32_t _bufferSize, uint32_t _maxUriChars)
{
	RTM_ASSERT(_uri, "");
	RTM_ASSERT(_buffer, "");

	uint32_t len = _maxUriChars == UINT32_MAX ? strLen(_uri) : _maxUriChars;
	const char* uriEnd = _uri + len;

	uint32_t dSize = 0;
	while (_uri < uriEnd)
	{
		if (dSize >= _bufferSize)
		{
			_buffer[0] = '\0';
			return UINT32_MAX;
		}

		char ch = *_uri++;

		if (ch == '%')
		{
			if (_uri > uriEnd - 2)
			{
				_buffer[0] = '\0';
				return UINT32_MAX;
			}

			if (isHexNum(_uri[0]) && isHexNum(_uri[1]))
			{
				char c1 = fromHexNum(*_uri++);
				char c2 = fromHexNum(*_uri++);
				char c = ((c1 << 4) | c2);

				_buffer[dSize++] = c;
			}
			else
			{
				_buffer[0] = '\0';	// invalid format or not enough space in destination buffer
				return 0;
			}
		}
		else
			_buffer[dSize++] = ch;
	}

	_buffer[dSize++] = '\0';

	return dSize;
}

} // namespace rtm
