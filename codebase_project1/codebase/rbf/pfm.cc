#include "pfm.h"
#include<stdio.h>
#include<unistd.h>

PagedFileManager* PagedFileManager::_pf_manager = 0;

PagedFileManager* PagedFileManager::instance()
{
    if(!_pf_manager)
        _pf_manager = new PagedFileManager();

    return _pf_manager;
}


PagedFileManager::PagedFileManager()
{
}


PagedFileManager::~PagedFileManager()
{
}


RC PagedFileManager::createFile(const string &fileName)
{
	if(0 == access(fileName.c_str(),F_OK))
	{
		return -1;
	}
	FILE *fp = NULL;
	fp = fopen(fileName.c_str(), "a+");
	if(NULL == fp)
	{
		return -1;
	}
	fclose(fp);
	fp = NULL;
    return 0;
}


RC PagedFileManager::destroyFile(const string &fileName)
{
	//if not exist
	if(-1 == access(fileName.c_str(),F_OK))
	{
		return -1;
	}
	if(-1 == remove(fileName.c_str()))
	{
		return -1;
	}

    return 0;
}


RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
	//if not exist
	if(-1 == access(fileName.c_str(),F_OK))
	{
		return -1;
	}
	FILE* fp = NULL;
	fp = fopen(fileName.c_str(), "r+");
	if(NULL == fp)
	{
		return -1;
	}else{
		fileHandle.setFileHandle(fp);
		fp = NULL;
	}
	return 0;
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
	FILE* fp = fileHandle.getFileHandle();
	if(NULL == fp)
	{
		return -1;
	}else{
		fclose(fp);
		fp = NULL;
		fileHandle.setFileHandle(NULL);
	}
    return 0;
}

FileHandle::FileHandle()
{
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;

    _fileHandle = NULL;
    _numberOfPages = 0;
}


FileHandle::~FileHandle()
{
}


RC FileHandle::readPage(PageNum pageNum, void *data)
{
	if(NULL == this->_fileHandle or pageNum >= this->_numberOfPages)
	{
		return -1;
	}
	if(0 != fseek(this->_fileHandle, pageNum*PAGE_SIZE, SEEK_SET))
	{
		return -1;
	}
	if(1 != fread(data, PAGE_SIZE, 1, this->_fileHandle))
	{
		return -1;
	}
	this->readPageCounter++;
	return 0;
}

RC FileHandle::writePage(PageNum pageNum, const void *data)
{
	if(NULL == this->_fileHandle or pageNum >= this->_numberOfPages)
	{
		return -1;
	}
	if(0 != fseek(this->_fileHandle, pageNum*PAGE_SIZE*1L, SEEK_SET))
	{
		return -1;
	}
	if(1 != fwrite(data, PAGE_SIZE, 1, this->_fileHandle))
	{
		return -1;
	}
	if(0 != fflush(this->_fileHandle))
	{
		return -1;
	}
	this->writePageCounter++;
	return 0;
}


RC FileHandle::appendPage(const void *data)
{
	if(NULL == this->_fileHandle){
		return -1;
	}
	if(0 != fseek(this->_fileHandle, 0L, SEEK_END))
	{
		return -1;
	}
	if(1 != fwrite(data, PAGE_SIZE, 1, this->_fileHandle))
	{
		return -1;
	}
	if(0 != fflush(this->_fileHandle))
	{
		return -1;
	}
	this->appendPageCounter++;
	this->_numberOfPages++;
    return 0;
}

unsigned FileHandle::getNumberOfPages()
{
	return this->_numberOfPages;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
	readPageCount = this->readPageCounter;
	writePageCount = this->writePageCounter;
	appendPageCount = this->appendPageCounter;

    return 0;
}

RC FileHandle::setFileHandle(FILE* fh)
{
	if(NULL == fh)
	{
		return -1;
	}
	this->_fileHandle = fh;
	//calculate the number of pages
	unsigned long filesize = -1;
	if(0 != fseek(this->_fileHandle, 0L, SEEK_END))
	{
		return -1;
	}
	filesize = ftell(this->_fileHandle);
	this->_numberOfPages = filesize/PAGE_SIZE;
	return 0;
}

FILE* FileHandle::getFileHandle()
{
	return this->_fileHandle;
}
