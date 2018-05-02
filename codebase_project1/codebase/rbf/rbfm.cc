#include "rbfm.h"
#include<stdio.h>
#include<unistd.h>
#include<math.h>
#include<stdlib.h>
#include<string.h>
#include<iostream>

RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = 0;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
    if(!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();

    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
}

RecordBasedFileManager::~RecordBasedFileManager()
{
}

RC RecordBasedFileManager::createFile(const string &fileName) {
	PagedFileManager *pfm = PagedFileManager::instance();
	return pfm->createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const string &fileName) {
	PagedFileManager *pfm = PagedFileManager::instance();
	return pfm->destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) {
	PagedFileManager *pfm = PagedFileManager::instance();
	return pfm->openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
	PagedFileManager *pfm = PagedFileManager::instance();
	return pfm->closeFile(fileHandle);
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
	//calculate the record's length
	int offset = 0;

	int nullFieldsIndicatorActualSize = ceil((double) recordDescriptor.size() / CHAR_BIT);
	unsigned char *nullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
	memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

	memcpy(nullsIndicator,(char *)data + offset, nullFieldsIndicatorActualSize);
	offset += nullFieldsIndicatorActualSize;

	bool nullBit = false;
	int iLength = 0;
	for(unsigned int iLoop = 0; iLoop < recordDescriptor.size(); iLoop++ ){
		nullBit = nullsIndicator[0] & (1 << (nullFieldsIndicatorActualSize*8 - 1 - iLoop));
		if (!nullBit) {
			switch(recordDescriptor[iLoop].type) {
			case TypeInt:
				offset += sizeof(int);
				break;
			case TypeReal:
				offset += sizeof(float);
				break;
			case TypeVarChar:
				memcpy(&iLength, (char *)data + offset, sizeof(int));
				offset += sizeof(int);
				offset += iLength;
				break;
			default:
				return -1;
			}
		}
	}
	unsigned int iRecSize = offset;
	//Insert the Record
	unsigned pageCount = fileHandle.getNumberOfPages();
	void *buffer = malloc(PAGE_SIZE);

	if(0 != pageCount){
		//there is available page
		if( 0 != fileHandle.readPage(pageCount - 1, buffer)){
			return -1;
		}
		unsigned int ifreespace = 0;
		//read the free space size
		memcpy(&ifreespace, (char*)buffer + PAGE_SIZE - sizeof(unsigned int), sizeof(unsigned int));
		if(ifreespace >= iRecSize + ((unsigned int)sizeof(IREC))){
			/*there is enough space*/
			//read the count of records
			int iRecCnt = 0;
			memcpy(&iRecCnt, (char*)buffer + PAGE_SIZE - 2* sizeof(int), sizeof(int));
			//calculate the offset to insert the new record
			int ioffset = PAGE_SIZE - ifreespace - 2 *sizeof(int) - iRecCnt *sizeof(IREC);
			memcpy((char*)buffer + ioffset, data, iRecSize);

			rid.pageNum = pageCount -1;
			rid.slotNum = iRecCnt;
			//calculate the free space size and the record size
			iRecCnt++;
			ifreespace = ifreespace - iRecSize - sizeof(IREC);
			memcpy((char*)buffer + PAGE_SIZE - sizeof(int), &ifreespace, sizeof(int));
			memcpy((char*)buffer + PAGE_SIZE - 2* sizeof(int), &iRecCnt, sizeof(int));
			//store the index of this record
			IREC indexRec;
			indexRec.isdelete = 0;
			indexRec.pos = ioffset;
			indexRec.size = iRecSize;
			memcpy((char*)buffer + PAGE_SIZE - 2* sizeof(int) - iRecCnt *sizeof(IREC), &indexRec, sizeof(IREC));
			if(0 != fileHandle.writePage(pageCount - 1, buffer)){
				return -1;
			}
			free(buffer);
			return 0;
		}
	}
	/*there is no page or there is no enough space*/
	//store the size of free space and the count of records
	int iRecCnt = 1;
	unsigned int ifreespace = PAGE_SIZE - 2*sizeof(int) - iRecSize - sizeof(IREC);
	memcpy((char*)buffer + PAGE_SIZE - sizeof(int), &ifreespace, sizeof(unsigned int));
	memcpy((char*)buffer + PAGE_SIZE - 2*sizeof(int), &iRecCnt, sizeof(int));
	//store the record
	memcpy((char *)buffer, (char *)data, iRecSize);
	//store the index of the record
	IREC indexRec;
	indexRec.pos = 0;
	indexRec.size = iRecSize;
	indexRec.isdelete = 0;
	memcpy((char*)buffer + PAGE_SIZE - 2*sizeof(int) - sizeof(IREC), &indexRec, sizeof(IREC));

	if(0 != fileHandle.appendPage(buffer)){
		return -1;
	}
	rid.pageNum = fileHandle.getNumberOfPages() -1;
	rid.slotNum = 0;

	free(buffer);
	return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {

	void *buffer = malloc(PAGE_SIZE);
	if( 0 != fileHandle.readPage(rid.pageNum, buffer)){
		return -1;
	}
	//read the index of record
	IREC indexRec;
	int offset = PAGE_SIZE - 2* sizeof(int) - (rid.slotNum + 1)* sizeof(IREC);
	memcpy(&indexRec, (char *)buffer + offset, sizeof(IREC));

	memcpy((char *)data, (char *)buffer + indexRec.pos, indexRec.size);
	free(buffer);

	return 0;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {

	int offset = 0;

    // Null-indicators
    int nullFieldsIndicatorActualSize = ceil((double) recordDescriptor.size() / CHAR_BIT);
    unsigned char *nullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

    // Null-indicator for the fields
	memcpy(nullsIndicator,(char *)data + offset, nullFieldsIndicatorActualSize);
	offset += nullFieldsIndicatorActualSize;

	// Beginning of the actual data
	// Note that the left-most bit represents the first field. Thus, the offset is 7 from right, not 0.
	// e.g., if a record consists of four fields and they are all nulls, then the bit representation will be: [11110000]
	bool nullBit = false;
	int iVar = 0;
	float fVar = 0.0;
	int iLength = 0;
	unsigned char* content;
	for(unsigned int iLoop = 0; iLoop < recordDescriptor.size(); iLoop++ )	{
		// Is this field not-NULL?
		nullBit = nullsIndicator[0] & (1 << (nullFieldsIndicatorActualSize*8 - 1 - iLoop));
		if (!nullBit) {
			switch(recordDescriptor[iLoop].type) {
			case TypeInt:
				memcpy( &iVar,(char *)data + offset, sizeof(int));
				offset += sizeof(int);
				cout<<recordDescriptor[iLoop].name<<" : "<<iVar<<endl;
				break;
			case TypeReal:
				memcpy( &fVar,(char *)data + offset, sizeof(int));
				offset += sizeof(float);
				cout<<recordDescriptor[iLoop].name<<" : "<<fVar<<endl;
				break;
			case TypeVarChar:
				memcpy(&iLength, (char *)data + offset, sizeof(int));
				offset += sizeof(int);
				content = (unsigned char *) malloc(iLength+1);
				memset(content, 0, iLength);
				memcpy(content, (char *)data + offset, iLength);
				content[iLength] = '\0';
				offset += iLength;
				cout<<recordDescriptor[iLoop].name<<" : "<<content<<endl;
				free(content);
				break;
			default:
				return -1;
			}
		}
	}
	return 0;
}
