#include "myfs.h"

bool MyFileSystem::isPowerOfTwo_(size_t num)
{
	if (!num)
	{
		return false;
	}
	while (!(num % 2))
	{
		num >>= 1;
	}
	return num == 1;
}

size_t MyFileSystem::strToLong_(const char* str)
{
	size_t length = std::strlen(str);
	size_t bitShift = 0;
	if (str[length - 1] == 'K' || str[length - 1] == 'M')
	{
		bitShift = str[length - 1] == 'K' ? 10 : 20;
		--length;
	}
	size_t result = 0, mult = 1;
	for (size_t i = length; i--;)
	{
		if (str[i] < '0' || str[i] > '9')
		{
			throw std::exception("Wrong string format");
		}
		result += (str[i] - '0') * mult;
		mult *= 10;
	}
	if (!isPowerOfTwo_(result))
	{
		throw std::exception("Wrong size format");
	}
	return result << bitShift;
}

size_t MyFileSystem::readLong_()
{
	int ch;
	size_t result = 0;
	for (size_t i = 0; i < 8; ++i)
	{
		if ((ch = mainFile_.get()) == EOF)
		{
			throw std::exception("Trying to read long after the end of the file");
		}
		result |= (ch << (i * 8));
	}
	return result;
}

size_t MyFileSystem::readLong_(size_t pos)
{
	mainFile_.seekg(pos, std::ios::beg);
	mainFile_.clear();
	return readLong_();
}

void MyFileSystem::writeLong_(size_t num)
{
	for (size_t i = 0; i < 8; ++i)
	{
		mainFile_.put(num >> (i * 8));
	}
}

void MyFileSystem::writeLong_(size_t num, size_t pos)
{
	mainFile_.seekp(pos, std::ios::beg);
	mainFile_.clear();
	writeLong_(num);
}

void MyFileSystem::rewriteBitNote_(size_t num, size_t index)
{
	bitMap_[index] = num;
	writeLong_(num, 8 + index * 8);
}

void MyFileSystem::readBitMap_()
{
	mainFile_.seekg(8, std::ios::beg);
	mainFile_.clear();
	for (size_t i = 0; i < blocksForData_; ++i)
	{
		bitMap_[i] = readLong_();
	}
}

void MyFileSystem::overwriteBitMap_()
{
	mainFile_.seekp(8, std::ios::beg);
	mainFile_.clear();
	for (size_t i = 0; i < blocksForData_; ++i)
	{
		writeLong_(bitMap_[i]);
	}
}

void MyFileSystem::overWriteFileService_()
{
	writeLong_(fileList_.staticMap_.size(), fileServiceBegin_);
	for (auto it = fileList_.staticMap_.begin(); it != fileList_.staticMap_.end(); ++it)
	{
		for (size_t i = 0; i < fileNameSize; ++i)
		{
			if (i < it->first.length())
			{
				mainFile_.put(it->first[i]);
			}
			else
			{
				mainFile_.put('\0');
			}
		}
		writeLong_(it->second.firstBlock_);
		writeLong_(it->second.byteCount_);
	}
}

void MyFileSystem::initServiceInfo_()
{
	blocksForData_ = blockCount_ - blocksForService_;
	fileServiceBegin_ = 8 + blocksForData_ * 8;
	fileList_.maxFileCount_ = (blocksForService_ * blockSize_ - fileServiceBegin_ - 8) / fileNoteSize;
}

void MyFileSystem::createService_()
{
	size_t minBytesForService = 8 + blockCount_ * 8 + minBytesForFileService;
	blocksForService_ = minBytesForService / blockSize_ + 2;
	if (blockCount_ / blocksForService_ < minServiceNAllBlocksDifference)
	{
		throw std::exception("Too many block for service purposes");
	}
	if (blockCount_ / blocksForService_ > optimalServiceNAllBlocksDifference)
	{
		blocksForService_ = blockCount_ / optimalServiceNAllBlocksDifference + 1;
	}
	initServiceInfo_();
	writeLong_(blocksForService_, 0);
	bitMap_ = new size_t[blocksForData_];
	for (size_t i = 0; i < blocksForData_; ++i)
	{
		bitMap_[i] = 0;
	}
	writeLong_(0, fileServiceBegin_);
	overwriteBitMap_();
}

void MyFileSystem::readService_()
{
	blocksForService_ = readLong_(0);
	initServiceInfo_();
	bitMap_ = new size_t[blocksForData_];
	for (size_t i = 0; i < blocksForData_; ++i)
	{
		bitMap_[i] = readLong_();
	}
	size_t fileCount = readLong_();
	for (size_t i = 0; i < fileCount; ++i)
	{
		int ch;
		std::string fileName;
		for (size_t j = 0; j < 32; ++j)
		{
			if ((ch = mainFile_.get()) == EOF)
			{
				throw std::exception("Trying to read filename after the end of file");
			}
			if (ch != '\0')
			{
				fileName.push_back(ch);
			}
		}
		FileList::fileNote note;
		note.firstBlock_ = readLong_();
		note.byteCount_ = readLong_();
		note.isOpened_ = false;
		fileList_.staticMap_.insert(std::make_pair(fileName, note));
	}
}

void MyFileSystem::beforeClosingFile_(const std::map<int, FileList::activeFileNote>::iterator& itAct)
{
	std::map<std::string, FileList::fileNote>::iterator itStat;
	if ((itStat = fileList_.staticMap_.find(itAct->second.fileName_)) == fileList_.staticMap_.end())
	{
		throw std::exception("Can not match open file by its name");
	}
	itStat->second.byteCount_ = itAct->second.byteCount_;
	itStat->second.isOpened_ = false;
}

size_t MyFileSystem::getLastBlock_(size_t firstBlock) const
{
	size_t index = firstBlock - blocksForService_;
	while (bitMap_[index] != 1)
	{
		if (!bitMap_[index])
		{
			throw std::exception("Bitmap is corrupted");
		}
		index = bitMap_[index] - blocksForService_;
	}
	return blocksForService_ + index;
}

bool MyFileSystem::findFreeBlockIndex_(size_t& resultIndex, size_t indexToStart) const
{
	for (size_t i = indexToStart; i < blocksForData_; ++i)
	{
		if (!bitMap_[i])
		{
			resultIndex = i;
			return true;
		}
	}
	return false;
}

bool MyFileSystem::findFreeBlockIndex_(size_t& resultIndex) const
{
	return findFreeBlockIndex_(resultIndex, 0);
}

void MyFileSystem::writeToBlockIndex_(size_t index, const char* buffer, size_t count)
{
	mainFile_.seekp((blocksForService_ + index) * blockSize_, std::ios::beg);
	mainFile_.clear();
	mainFile_.write(buffer, count);
}

void MyFileSystem::readFromBlock_(size_t block, char* buffer, size_t count)
{
	mainFile_.seekg(block * blockSize_, std::ios::beg);
	mainFile_.clear();
	mainFile_.read(buffer, count);
}

MyFileSystem::MyFileSystem(const char* fileName, const char* fileSize, const char* blockSize)
{
	mainFileSize_ = strToLong_(fileSize);
	blockSize_ = strToLong_(blockSize);
	if (blockSize_ >= mainFileSize_)
	{
		throw std::exception("Block size is bigger than file system size");
	}
	if (mainFileSize_ < minFileSystemSize)
	{
		throw std::exception("Too little file system size");
	}
	if (blockSize_ < minBlockSize)
	{
		throw std::exception("Too little block size");
	}
	if (mainFileSize_ / blockSize_ < minFileSystemNBlockDifference)
	{
		throw std::exception("Too big block size");
	}
	mainFile_.open(fileName);
	bool fileCreated = false;
	if (!mainFile_)
	{
		std::ofstream outFile(fileName);
		if (!outFile)
		{
			throw std::exception("Can not create file for file system");
		}
		outFile.seekp(mainFileSize_ - 1, std::ios::beg);
		outFile.write("", 1);
		outFile.close();
		mainFile_.open(fileName);
		if (!mainFile_)
		{
			throw std::exception("Can not open just created file for file system");
		}
		fileCreated = true;
	}
	mainFile_.seekg(0, std::ios::end);
	if (mainFile_.tellg() != mainFileSize_)
	{
		mainFile_.close();
		throw std::exception("Given file size does not equal actual file size");
	}
	mainFile_.seekg(0, std::ios::beg);
	mainFile_.clear();
	mainFile_.seekp(0, std::ios::beg);
	blockCount_ = mainFileSize_ / blockSize_;
	fileList_.maxID_ = 0;
	if (fileCreated)
	{
		createService_();
	}
	else
	{
		readService_();
	}
}

MyFileSystem::~MyFileSystem()
{
	for (auto it = fileList_.activeMap_.begin(); it != fileList_.activeMap_.end(); ++it)
	{
		beforeClosingFile_(it);
	}
	overWriteFileService_();
	delete[] bitMap_;
	mainFile_.close();
}

void MyFileSystem::printMainInfo() const
{
	std::cout << "File system size: " << mainFileSize_ << std::endl;
	std::cout << "Block size: " << blockSize_ << std::endl;
	std::cout << "Blocks count: " << blockCount_ << std::endl;
	std::cout << "Service blocks count: " << blocksForService_ << std::endl;
	std::cout << "Data blocks count: " << blocksForData_ << std::endl;
	std::cout << "Files count: " << fileList_.staticMap_.size() << std::endl;
	std::cout << "Max files count: " << fileList_.maxFileCount_ << std::endl;
	std::cout << "Open files count: " << fileList_.activeMap_.size() << std::endl << std::endl;;
}

void MyFileSystem::printFileInfo() const
{
	std::cout << "Total " << fileList_.staticMap_.size() << " files" << std::endl;
	for (auto it = fileList_.staticMap_.begin(); it != fileList_.staticMap_.end(); ++it)
	{
		std::cout << it->first << " - First block: " << it->second.firstBlock_ << ", Size: " << it->second.byteCount_ << std::endl;
	}
	std::cout << std::endl << fileList_.activeMap_.size() << " open files" << std::endl;
	for (auto it = fileList_.activeMap_.begin(); it != fileList_.activeMap_.end(); ++it)
	{
		std::cout << "FD:" << it->first << " - Filename: " << it->second.fileName_ << ", Size: " << it->second.byteCount_
			<< ", Read pointer: " << it->second.readPointer_ << ", Block read: " << it->second.curBlockToRead_ << std::endl;
	}
	std::cout << std::endl;
}

void MyFileSystem::printSimpleBitMap() const
{
	std::cout << "Simple Bitmap:" << std::endl;
	for (size_t i = 0; i < blocksForData_; ++i)
	{
		if (bitMap_[i] < 2)
		{
			std::cout << bitMap_[i];
		}
		else
		{
			std::cout << 2;
		}
	}
	std::cout << std::endl << std::endl;
}

void MyFileSystem::printAdvancedBitMap() const
{
	std::cout << "Advanced Bitmap:" << std::endl;
	for (size_t i = 0; i < blocksForData_; ++i)
	{
		std::cout << bitMap_[i] << ' ';
	}
	std::cout << std::endl << std::endl;
}

int MyFileSystem::create(const std::string& fileName)
{
	if (fileName.size() > 32)
	{
		return -1;
	}
	if (fileList_.staticMap_.find(fileName) != fileList_.staticMap_.end())
	{
		return -1;
	}
	if (fileList_.staticMap_.size() >= fileList_.maxFileCount_)
	{
		return -1;
	}
	size_t freeBlockIndex;
	if (!findFreeBlockIndex_(freeBlockIndex))
	{
		return -1;
	}
	rewriteBitNote_(1, freeBlockIndex);
	FileList::fileNote note = { blocksForService_ + freeBlockIndex, 0, false };
	fileList_.staticMap_.insert(std::make_pair(fileName, note));
	return 0;
}

int MyFileSystem::delete_(const std::string& fileName)
{
	auto it = fileList_.staticMap_.find(fileName);
	if (it == fileList_.staticMap_.end())
	{
		return -1;
	}
	if (it->second.isOpened_)
	{
		return -1;
	}
	size_t index = it->second.firstBlock_ - blocksForService_;
	size_t tmp;
	while (bitMap_[index] != 1)
	{
		if (!bitMap_[index])
		{
			throw std::exception("Bitmap is corrupted");
		}
		tmp = bitMap_[index] - blocksForService_;
		rewriteBitNote_(0, index);
		index = tmp;
	}
	rewriteBitNote_(0, index);
	fileList_.staticMap_.erase(it);
	return 0;
}

int MyFileSystem::open(const std::string& fileName)
{
	std::map<std::string, FileList::fileNote>::iterator it;
	if ((it = fileList_.staticMap_.find(fileName)) == fileList_.staticMap_.end())
	{
		return -1;
	}
	if (++fileList_.maxID_ < 0)
	{
		throw std::exception("Number of opening operations exceeded. Reboot file system");
	}
	FileList::activeFileNote note = { fileName, it->second.firstBlock_, it->second.byteCount_, 0, it->second.firstBlock_, getLastBlock_(it->second.firstBlock_) };
	fileList_.activeMap_.insert(std::make_pair(fileList_.maxID_, note));
	it->second.isOpened_ = true;
	return fileList_.maxID_;
}

int MyFileSystem::close(int fd)
{
	std::map<int, FileList::activeFileNote>::iterator it;
	if ((it = fileList_.activeMap_.find(fd)) == fileList_.activeMap_.end())
	{
		return -1;
	}
	beforeClosingFile_(it);
	fileList_.activeMap_.erase(it);
}

int MyFileSystem::write(int fd, const char* buffer, size_t size)
{
	std::map<int, FileList::activeFileNote>::iterator it;
	if ((it = fileList_.activeMap_.find(fd)) == fileList_.activeMap_.end())
	{
		return -1;
	}
	size_t writeToCurBlock = ((it->second.byteCount_ % blockSize_) || !it->second.byteCount_) ? (blockSize_ - (it->second.byteCount_ % blockSize_)) : 0;
	size_t curBlock = it->second.lastBlock_;
	if (writeToCurBlock)
	{
		mainFile_.seekp(curBlock * blockSize_ + (it->second.byteCount_ % blockSize_), std::ios::beg);
		mainFile_.clear();
		if (size <= writeToCurBlock)
		{
			mainFile_.write(buffer, size);
			it->second.byteCount_ += size;
			return 0;
		}
		mainFile_.write(buffer, writeToCurBlock);
	}
	int bytesWritten = writeToCurBlock;
	size_t curBlockIndex, prevBlockIndex = curBlock - blocksForService_;
	if (!findFreeBlockIndex_(curBlockIndex))
	{
		it->second.byteCount_ += bytesWritten;
		return bytesWritten;
	}
	rewriteBitNote_(blocksForService_ + curBlockIndex, prevBlockIndex);
	rewriteBitNote_(1, curBlockIndex);
	prevBlockIndex = curBlockIndex;
	int toWrite = (size - bytesWritten) < blockSize_ ? (size - bytesWritten) : blockSize_;
	writeToBlockIndex_(curBlockIndex, buffer + bytesWritten, toWrite);
	bytesWritten += toWrite;
	while (bytesWritten < size)
	{
		if (!findFreeBlockIndex_(curBlockIndex, prevBlockIndex))
		{
			it->second.byteCount_ += bytesWritten;
			return bytesWritten;
		}
		rewriteBitNote_(blocksForService_ + curBlockIndex, prevBlockIndex);
		rewriteBitNote_(1, curBlockIndex);
		prevBlockIndex = curBlockIndex;
		toWrite = (size - bytesWritten) < blockSize_ ? (size - bytesWritten) : blockSize_;
		writeToBlockIndex_(curBlockIndex, buffer + bytesWritten, toWrite);
		bytesWritten += toWrite;
	}
	it->second.byteCount_ += bytesWritten;
	it->second.lastBlock_ = blocksForService_ + curBlockIndex;
	return 0;
}

int MyFileSystem::read(int fd, char* buffer, size_t size)
{
	std::map<int, FileList::activeFileNote>::iterator it;
	if ((it = fileList_.activeMap_.find(fd)) == fileList_.activeMap_.end())
	{
		return -1;
	}
	if (it->second.readPointer_ == it->second.byteCount_)
	{
		return -1;
	}
	size_t readFromCurBlock;
	std::cout << it->second.curBlockToRead_ << ' ' << it->second.lastBlock_ << std::endl;
	if (it->second.curBlockToRead_ == it->second.lastBlock_)
	{
		readFromCurBlock = ((it->second.byteCount_ % blockSize_) ? it->second.byteCount_ % blockSize_ : blockSize_) - (it->second.readPointer_ % blockSize_);
	}
	else
	{
		readFromCurBlock = blockSize_ - (it->second.readPointer_ % blockSize_);
	}
	mainFile_.seekg(it->second.curBlockToRead_ * blockSize_ + (it->second.readPointer_ % blockSize_), std::ios::beg);
	mainFile_.clear();
	if (size <= readFromCurBlock)
	{
		mainFile_.read(buffer, size);
		it->second.readPointer_ += size;
		if (size == readFromCurBlock)
		{
			it->second.curBlockToRead_ = bitMap_[it->second.curBlockToRead_ - blocksForService_];
		}
		return 0;
	}
	mainFile_.read(buffer, readFromCurBlock);
	int bytesRead = readFromCurBlock;
	it->second.readPointer_ += bytesRead;
	while (bytesRead < size)
	{
		if (!bitMap_[it->second.curBlockToRead_ - blocksForService_])
		{
			throw std::exception("Bitmap is corrupted");
		}
		if (bitMap_[it->second.curBlockToRead_ - blocksForService_] == 1)
		{
			return bytesRead;
		}
		it->second.curBlockToRead_ = bitMap_[it->second.curBlockToRead_ - blocksForService_];
		readFromCurBlock = size - bytesRead > blockSize_ ? blockSize_ : size - bytesRead;
		if (bitMap_[it->second.curBlockToRead_ - blocksForService_] == 1)
		{
			size_t lastBLockSize = (it->second.byteCount_ % blockSize_) ? (it->second.byteCount_ % blockSize_) : blockSize_;
			readFromCurBlock = readFromCurBlock > lastBLockSize ? lastBLockSize : readFromCurBlock;
		}
		readFromBlock_(it->second.curBlockToRead_, buffer + bytesRead, readFromCurBlock);
		bytesRead += readFromCurBlock;
		it->second.readPointer_ += readFromCurBlock;
	}
	if (readFromCurBlock == blockSize_ && it->second.curBlockToRead_ != it->second.lastBlock_)
	{
		it->second.curBlockToRead_ = bitMap_[it->second.curBlockToRead_ - blocksForService_];
	}
	return 0;
}