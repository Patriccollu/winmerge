//
// File_WIN32U.cpp
//
// $Id: //poco/1.4/Foundation/src/File_WIN32U.cpp#1 $
//
// Library: Foundation
// Package: Filesystem
// Module:  File
//
// Copyright (c) 2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#include "Poco/File_WIN32U.h"
#include "Poco/Exception.h"
#include "Poco/String.h"
#include "Poco/UnicodeConverter.h"
#include "Poco/UnWindows.h"


namespace Poco {


class FileHandle
{
public:
	FileHandle(const std::string& path, const std::wstring& upath, DWORD access, DWORD share, DWORD disp)
	{
		_h = CreateFileW(upath.c_str(), access, share, 0, disp, 0, 0);
		if (_h == INVALID_HANDLE_VALUE)
		{
			FileImpl::handleLastErrorImpl(path);
		}
	}
	
	~FileHandle()
	{
		if (_h != INVALID_HANDLE_VALUE) CloseHandle(_h);
	}
	
	HANDLE get() const
	{
		return _h;
	}
	
private:
	HANDLE _h;
};


FileImpl::FileImpl()
{
}


FileImpl::FileImpl(const std::string& path): _path(path)
{
	std::string::size_type n = _path.size();
	if (n > 1 && (_path[n - 1] == '\\' || _path[n - 1] == '/') && !((n == 3 && _path[1]==':')))
	{
		_path.resize(n - 1);
	}
	convertPath(_path, _upath);
}


FileImpl::~FileImpl()
{
}


void FileImpl::swapImpl(FileImpl& file)
{
	std::swap(_path, file._path);
	std::swap(_upath, file._upath);
}


void FileImpl::setPathImpl(const std::string& path)
{
	_path = path;
	std::string::size_type n = _path.size();
	if (n > 1 && (_path[n - 1] == '\\' || _path[n - 1] == '/') && !((n == 3 && _path[1]==':')))
	{
		_path.resize(n - 1);
	}
	convertPath(_path, _upath);
}


bool FileImpl::existsImpl() const
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributesW(_upath.c_str());
	if (attr == 0xFFFFFFFF)
	{
		switch (GetLastError())
		{
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
		case ERROR_NOT_READY:
		case ERROR_INVALID_DRIVE:
			return false;
		default:
			handleLastErrorImpl(_path);
		}
	}
	return true;
}


bool FileImpl::canReadImpl() const
{
	poco_assert (!_path.empty());
	
	DWORD attr = GetFileAttributesW(_upath.c_str());
	if (attr == 0xFFFFFFFF)
	{
		switch (GetLastError())
		{
		case ERROR_ACCESS_DENIED:
			return false;
		default:
			handleLastErrorImpl(_path);
		}
	}
	return true;
}


bool FileImpl::canWriteImpl() const
{
	poco_assert (!_path.empty());
	
	DWORD attr = GetFileAttributesW(_upath.c_str());
	if (attr == 0xFFFFFFFF)
		handleLastErrorImpl(_path);
	return (attr & FILE_ATTRIBUTE_READONLY) == 0;
}


bool FileImpl::canExecuteImpl() const
{
	Path p(_path);
	return icompare(p.getExtension(), "exe") == 0;
}


bool FileImpl::isFileImpl() const
{
	return !isDirectoryImpl() && !isDeviceImpl();
}


bool FileImpl::isDirectoryImpl() const
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributesW(_upath.c_str());
	if (attr == 0xFFFFFFFF)
		handleLastErrorImpl(_path);
	return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}


bool FileImpl::isLinkImpl() const
{
	return false;
}


bool FileImpl::isDeviceImpl() const
{
	poco_assert (!_path.empty());

	FileHandle fh(_path, _upath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING);
	DWORD type = GetFileType(fh.get());
	if (type == FILE_TYPE_CHAR)
		return true;
	else if (type == FILE_TYPE_UNKNOWN && GetLastError() != NO_ERROR)
		handleLastErrorImpl(_path);
	return false;
}


bool FileImpl::isHiddenImpl() const
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributesW(_upath.c_str());
	if (attr == 0xFFFFFFFF)
		handleLastErrorImpl(_path);
	return (attr & FILE_ATTRIBUTE_HIDDEN) != 0;
}


Timestamp FileImpl::createdImpl() const
{
	poco_assert (!_path.empty());

	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (GetFileAttributesExW(_upath.c_str(), GetFileExInfoStandard, &fad) == 0) 
		handleLastErrorImpl(_path);
	return Timestamp::fromFileTimeNP(fad.ftCreationTime.dwLowDateTime, fad.ftCreationTime.dwHighDateTime);
}


Timestamp FileImpl::getLastModifiedImpl() const
{
	poco_assert (!_path.empty());

	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (GetFileAttributesExW(_upath.c_str(), GetFileExInfoStandard, &fad) == 0) 
		handleLastErrorImpl(_path);
	return Timestamp::fromFileTimeNP(fad.ftLastWriteTime.dwLowDateTime, fad.ftLastWriteTime.dwHighDateTime);
}


void FileImpl::setLastModifiedImpl(const Timestamp& ts)
{
	poco_assert (!_path.empty());

	UInt32 low;
	UInt32 high;
	ts.toFileTimeNP(low, high);
	FILETIME ft;
	ft.dwLowDateTime  = low;
	ft.dwHighDateTime = high;
	FileHandle fh(_path, _upath, FILE_ALL_ACCESS, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING);
	if (SetFileTime(fh.get(), 0, &ft, &ft) == 0)
		handleLastErrorImpl(_path);
}


FileImpl::FileSizeImpl FileImpl::getSizeImpl() const
{
	poco_assert (!_path.empty());

	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (GetFileAttributesExW(_upath.c_str(), GetFileExInfoStandard, &fad) == 0) 
		handleLastErrorImpl(_path);
	LARGE_INTEGER li;
	li.LowPart  = fad.nFileSizeLow;
	li.HighPart = fad.nFileSizeHigh;
	return li.QuadPart;
}


void FileImpl::setSizeImpl(FileSizeImpl size)
{
	poco_assert (!_path.empty());

	FileHandle fh(_path, _upath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING);
	LARGE_INTEGER li;
	li.QuadPart = size;
	if (SetFilePointer(fh.get(), li.LowPart, &li.HighPart, FILE_BEGIN) == -1)
		handleLastErrorImpl(_path);
	if (SetEndOfFile(fh.get()) == 0) 
		handleLastErrorImpl(_path);
}


void FileImpl::setWriteableImpl(bool flag)		
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributesW(_upath.c_str());
	if (attr == -1)
		handleLastErrorImpl(_path);
	if (flag)
		attr &= ~FILE_ATTRIBUTE_READONLY;
	else
		attr |= FILE_ATTRIBUTE_READONLY;
	if (SetFileAttributesW(_upath.c_str(), attr) == 0)
		handleLastErrorImpl(_path);
}


void FileImpl::setExecutableImpl(bool flag)
{
	// not supported
}


void FileImpl::copyToImpl(const std::string& path) const
{
	poco_assert (!_path.empty());

	std::wstring upath;
	convertPath(path, upath);
	if (CopyFileW(_upath.c_str(), upath.c_str(), FALSE) == 0)
		handleLastErrorImpl(_path);
}


void FileImpl::renameToImpl(const std::string& path)
	{
	poco_assert (!_path.empty());

	std::wstring upath;
	convertPath(path, upath);
	if (MoveFileExW(_upath.c_str(), upath.c_str(), MOVEFILE_REPLACE_EXISTING) == 0)
		handleLastErrorImpl(_path);
}


void FileImpl::removeImpl()
{
	poco_assert (!_path.empty());

	if (isDirectoryImpl())
	{
		if (RemoveDirectoryW(_upath.c_str()) == 0) 
			handleLastErrorImpl(_path);
	}
	else
	{
		if (DeleteFileW(_upath.c_str()) == 0)
			handleLastErrorImpl(_path);
	}
}


bool FileImpl::createFileImpl()
{
	poco_assert (!_path.empty());

	HANDLE hFile = CreateFileW(_upath.c_str(), GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile);
		return true;
	}
	else if (GetLastError() == ERROR_FILE_EXISTS)
		return false;
	else
		handleLastErrorImpl(_path);
	return false;
}


bool FileImpl::createDirectoryImpl()
{
	poco_assert (!_path.empty());
	
	if (existsImpl() && isDirectoryImpl())
		return false;
	if (CreateDirectoryW(_upath.c_str(), 0) == 0)
		handleLastErrorImpl(_path);
	return true;
}


void FileImpl::handleLastErrorImpl(const std::string& path)
{
	switch (GetLastError())
	{
	case ERROR_FILE_NOT_FOUND:
		throw FileNotFoundException(path);
	case ERROR_PATH_NOT_FOUND:
	case ERROR_BAD_NETPATH:
	case ERROR_CANT_RESOLVE_FILENAME:
	case ERROR_INVALID_DRIVE:
		throw PathNotFoundException(path);
	case ERROR_ACCESS_DENIED:
		throw FileAccessDeniedException(path);
	case ERROR_ALREADY_EXISTS:
	case ERROR_FILE_EXISTS:
		throw FileExistsException(path);
	case ERROR_INVALID_NAME:
	case ERROR_DIRECTORY:
	case ERROR_FILENAME_EXCED_RANGE:
	case ERROR_BAD_PATHNAME:
		throw PathSyntaxException(path);
	case ERROR_FILE_READ_ONLY:
		throw FileReadOnlyException(path);
	case ERROR_CANNOT_MAKE:
		throw CreateFileException(path);
	case ERROR_DIR_NOT_EMPTY:
		throw FileException("directory not empty", path);
	case ERROR_WRITE_FAULT:
		throw WriteFileException(path);
	case ERROR_READ_FAULT:
		throw ReadFileException(path);
	case ERROR_SHARING_VIOLATION:
		throw FileException("sharing violation", path);
	case ERROR_LOCK_VIOLATION:
		throw FileException("lock violation", path);
	case ERROR_HANDLE_EOF:
		throw ReadFileException("EOF reached", path);
	case ERROR_HANDLE_DISK_FULL:
	case ERROR_DISK_FULL:
		throw WriteFileException("disk is full", path);
	case ERROR_NEGATIVE_SEEK:
		throw FileException("negative seek", path);
	default:
		throw FileException(path);
	}
}


void FileImpl::convertPath(const std::string& utf8Path, std::wstring& utf16Path)
{
	UnicodeConverter::toUTF16(utf8Path, utf16Path);
	if (utf16Path.size() > MAX_PATH - 12) // Note: CreateDirectory has a limit of MAX_PATH - 12 (room for 8.3 file name)
	{
		if (utf16Path[0] == '\\' || utf16Path[1] == ':')
		{
			if (utf16Path.compare(0, 4, L"\\\\?\\", 4) != 0)
			{
				if (utf16Path[1] == '\\')
					//	Starting with  "\\\\server\\etc\\etc..."
					//	Form resulting "\\\\?\\UNC\\server\\etc\\etc..."
					utf16Path.insert(1, L"\\?\\UNC", 6);
				else
					//	Starting with  "C:\\etc\\etc..."
					//	Form resulting "\\\\?\\C:\\etc\\etc..."
					utf16Path.insert(0, L"\\\\?\\", 4);
			}
		}
	}
	else
	{
		if (utf16Path.compare(0, 8, L"\\\\?\\UNC\\", 8) == 0)
		{
			utf16Path.erase(1, 6);	// leave "\\\\" at beginning
		}
		else
		if (utf16Path.compare(0, 4, L"\\\\?\\", 4) == 0)
		{
			utf16Path.erase(0, 4);
		}
	}
}

} // namespace Poco
