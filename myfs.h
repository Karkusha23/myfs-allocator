#pragma once
#include <iostream>
#include <fstream>
#include <exception>
#include <string>
#include <map>

// Файловая система делится на блоки, размер которых передается в конструкторе
// И размер файловой системы, и размер блока являются степенями двойки
// В соответствии с алгоритмом первые несколько блоков в начале выделенного файла выделяются под служебные нужды
// Первые 8 байтов выделенного под систему файла содержат количество блоков для служебной информации
// Далее содержится массив (битмап) 8-байтных чисел, каждое из которых соответствуют одному блоку по порядку их следования
// Размер массива фиксирован и равняется количеству блоков для данных
// 0 - блок свободен, 1 - блок является последним для файла, *номер другого блока* - номер следующего блока для файла
// Однозначность достигается засчет того, что служебная информация всегда занимает минимум два блока
// Оставшееся место выделено для сохранения информации о файлах, 8 байтов на количество файлов, далее 48 байтов на один файл
// 32 байта - имя файла, 8 байтов - номер первого блока файла, 8 байтов - количество байтов в файле

// Работа с файлами осуществляется засчет двух контейнеров std::map - одного для всех файлов, другого - только для открытых
// Первый сопоставляет имени файла информацию о его первом блоке, размеру и статусу (открыт / не открыт)
// Второй сопоставляет дескриптору открытого файла его имя, номер первого блока, размер, позиция чтения, номер читаемого блока, номер послденего блока файла

// При изменении битмапа изменения сразу заносятся в файл, в то время как запись обновленной информации о файлах производится только при вызове деструктора класса
// Также при вызове деструктора и закрытии файла производится запись обновленной информации из контейнера открытых файлов в контейнер всех файлов

// Далее под номером блока будем подразумевать его абсолютный номер, а под индексом блока - его номер относительно начала пользовательких данных
// Таким образом, (индекс блока) = (номер блока) - (кол-во служебных блоков)

// Минимальный размер файловой системы
const size_t minFileSystemSize = 1 << 20;

// Минимальный размер блока
const size_t minBlockSize = 256;

// Минимально возможное соотношение размера файловой системы к размеру блока
const size_t minFileSystemNBlockDifference = 8;

// Минимальное гарантированное число файлов, которое может предоставить файловая система
const size_t minFileCount = 20;

// Минимальное количество байт, отделяемое для информации о файлах
const size_t minBytesForFileService = 8 + 48 * minFileCount;

// Минимально возможное соотношение общего числа блоков к числу блоков для служебной информации
const size_t minServiceNAllBlocksDifference = 6;

// Оптимальное соотношение общего числа блоков к числу блоков для служебной информации
const size_t optimalServiceNAllBlocksDifference = 16;

// Количество байтов, отведенное для имени файла
const size_t fileNameSize = 32;

// Количество байтов, отведенное для одной записи о файле
const size_t fileNoteSize = fileNameSize + 8 * 2;

class MyFileSystem
{
private:
	class FileList
	{
	public:
		struct fileNote
		{
			size_t firstBlock_;
			size_t byteCount_;
			bool isOpened_;
		};
		struct activeFileNote
		{
			std::string fileName_;
			size_t firstBlock_;
			size_t byteCount_;
			size_t readPointer_;
			size_t curBlockToRead_;
			size_t lastBlock_;
		};
		std::map<std::string, fileNote> staticMap_;
		std::map<int, activeFileNote> activeMap_;
		size_t maxFileCount_;
		int maxID_;
	} fileList_;
	std::fstream mainFile_;
	size_t mainFileSize_;
	size_t blockSize_;
	size_t blockCount_;
	size_t blocksForService_;
	size_t blocksForData_;
	size_t fileServiceBegin_;
	size_t* bitMap_;
	static bool isPowerOfTwo_(size_t num); // Является ли число степенью двойки
	static size_t strToLong_(const char* str); // Перевести строку указанного в задании формата в size_t в байтах
	size_t readLong_(); // Прочитать 8 байт из файла системы в size_t, начиная с текущей позиции чтения
	size_t readLong_(size_t pos); // Прочитать 8 байт из файла системы в size_t, начиная с позиции pos
	void writeLong_(size_t num); // Записать size_t в файл системы, начиная с текущей позиции записи
	void writeLong_(size_t num, size_t pos); // Записать size_t в файл системы, начиная с позиции pos
	void rewriteBitNote_(size_t num, size_t index); // Изменяет значение элемента битмапа с индексом index на num одновременно и в оперативной памяти, и в файле
	void readBitMap_(); // Читает битмап из файла в оперативную память
	void overwriteBitMap_(); // Полностью переписывает битмап из оперативной памяти в файл
	void overWriteFileService_(); // Перезаписывает данные о файлах из оперативной памяти в файл
	void initServiceInfo_(); // Инициализирует переменные, относящиеся к служебным данным, после инициализации количество блоков данных
	void createService_(); // Инициализация служебной информации при создании файловой системы 
	void readService_(); // Инициализация служебной информации при чтении файловой системы из файла
	void beforeClosingFile_(const std::map<int, FileList::activeFileNote>::iterator& itAct); // Вызывается перед закрытием файла, переписывает данные открытого файла в контейнер всех файлов
	size_t getLastBlock_(size_t firstBlock) const; // Возвращает номер последнего блока файла по номеру его первого блока
	bool findFreeBlockIndex_(size_t& resultIndex, size_t startFrom) const; // Находит индекс первого свободного блока, начиная с блока с индексом startFrom, возвращает true, если нашел
	bool findFreeBlockIndex_(size_t& resultIndex) const; // Находит индекс первого свободного блока
	void writeToBlockIndex_(size_t index, const char* buffer, size_t count); // Записать count байтов в блок с индексом index, начиная с его начала
	void readFromBlock_(size_t block, char* buffer, size_t count); // Прочитать count байтов из блока с номером block, начиная с его начала
public:
	MyFileSystem(const char* fileName, const char* fileSize, const char* blockSize);
	~MyFileSystem();
	MyFileSystem(const MyFileSystem&) = delete;
	MyFileSystem(MyFileSystem&&) = delete;
	MyFileSystem& operator=(const MyFileSystem&) = delete;
	MyFileSystem& operator=(MyFileSystem&&) = delete;
	void printMainInfo() const; // Вывод общей информации о файловой системе
	void printFileInfo() const; // Вывод информации о файлах
	void printSimpleBitMap() const; // Упрощенный вывод битмапа
	void printAdvancedBitMap() const; // Полный вывод битмапа
	int create(const std::string& fileName);
	int delete_(const std::string& fileName);
	int open(const std::string& fileName);
	int close(int fd);
	int write(int fd, const char* buffer, size_t size);
	int read(int fd, char* buffer, size_t size);
};