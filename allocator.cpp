#include "allocator.h"

Allocator::Allocator(void* mem, size_t totalBytes)
{
	memByte_ = static_cast<unsigned char*>(mem);
	memLong_ = static_cast<size_t*>(mem);
	sizeByte_ = totalBytes;
	if (sizeByte_ < 33)
	{
		throw std::exception("Too little amount of memory");
	}
	memLong_[0] = 0;
}

size_t Allocator::getLongCount_(size_t bytesCount)
{
	return bytesCount / 8 + bool(bytesCount % 8);
}

size_t* Allocator::findFree_(size_t bytesCount, size_t*& prev, size_t*& next) const
{
	if (!memLong_[0]) // Первый участок
	{
		if (bytesCount > sizeByte_ - 32)
		{
			return nullptr;
		}
		prev = memLong_ + 1;
		next = nullptr;
		return memLong_ + 2;
	}
	size_t* left = memLong_ + 1, *right = reinterpret_cast<size_t*>(*left);
	if ((right - left) * 8 >= bytesCount + 24) // Проверка участка между заголовком области и крайним левым участком
	{
		prev = left;
		next = right;
		return left + 1;
	}
	for (size_t i = 0; i < memLong_[0]; ++i) // Проверка между каджыми двумя участками
	{
		left = right;
		right = reinterpret_cast<size_t*>(*left);
		size_t leftLongCount = getLongCount_(*(left + 1));
		if (right) // Если участок не крайний правый
		{
			if (right - left - 2 - leftLongCount >= getLongCount_(bytesCount) + 2)
			{
				prev = left;
				next = right;
				return left + 2 + leftLongCount;
			}
		}
		else // Если участок крайний правый
		{
			if ((sizeByte_ % 8) && !(sizeByte_ / 8 - (left + 2 - memLong_ + (*(left + 1) / 8)))) // Если справа остается участок меньше восьми байтов 
			{
				return nullptr;
			}
			if (sizeByte_ - (left + 2 + getLongCount_(*(left + 1)) - memLong_) * 8 >= bytesCount + 16)
			{
				prev = left;
				next = nullptr;
				return left + 2 + leftLongCount;
			}
		}
	}
	return nullptr;
}

void* Allocator::allocate(size_t numBytes)
{
	if (!numBytes)
	{
		throw std::bad_alloc();
	}
	size_t* cur, *left, *right;
	if (!(cur = findFree_(numBytes, left, right)))
	{
		throw std::bad_alloc();
	}
	*left = reinterpret_cast<size_t>(cur); // Изменение значения адреса следующего участка у левого соседа
	*cur = reinterpret_cast<size_t>(right); // Установка значения адреса следующего участка у текущего участка адреса правого участка
	*(cur + 1) = numBytes; // Установка значения размера участка текущему участку
	++memLong_[0];
	return cur + 2; // Возвращается указатель на начало пользовательских данных
}

void Allocator::deallocate(void* ptr)
{
	if (!memLong_[0])
	{
		throw std::exception("Nothing to deallocate");
	}
	size_t* toDelete = reinterpret_cast<size_t*>(ptr) - 2; // Адрес начала заголовка участка, подлежащего удалению
	size_t* cur = memLong_ + 1; // Адрес первого участка
	size_t* prev = nullptr;
	if (cur == toDelete) // Проверка переданного адреса на корректность
	{
		throw std::exception("Trying to deallocate by wrong pointer");
	}
	while (cur != toDelete) // Проверка переданного адреса на корректность 
	{
		prev = cur; // Запоминаем левого соседа
		cur = reinterpret_cast<size_t*>(*cur);
		if (!cur) // Дошли до крайнего правого, и ни один из просмотренных не совпал с переданным
		{
			throw std::exception("Trying to deallocate by wrong pointer");
		}
	}
	*prev = *cur; // Передаем адрес следующего участка левому соседу
	--memLong_[0];
}

std::string Allocator::bitmap() const
{
	std::string str;
	for (size_t i = 0; i < sizeByte_; ++i)
	{
		str.push_back('f');
	}
	for (size_t i = 0; i < 16; ++i)
	{
		str[i] = 's';
	}
	size_t* ptr = reinterpret_cast<size_t*>(memLong_[1]);
	for (size_t i = 0; i < memLong_[0]; ++i)
	{
		size_t left = reinterpret_cast<unsigned char*>(ptr) - memByte_;
		size_t right = left + 16;
		for (size_t j = left; j < right; ++j)
		{
			str[j] = 's';
		}
		left += 16;
		right += *(ptr + 1);
		for (size_t j = left; j < right; ++j)
		{
			str[j] = 'u';
		}
		ptr = reinterpret_cast<size_t*>(*ptr);
	}
	return str;
}

std::string Allocator::shortBitmap() const
{
	std::string str;
	size_t size = getLongCount_(sizeByte_);
	for (size_t i = 0; i < size; ++i)
	{
		str.push_back('f');
	}
	str[0] = str[1] = 's';
	size_t* ptr = reinterpret_cast<size_t*>(memLong_[1]);
	for (size_t i = 0; i < memLong_[0]; ++i)
	{
		size_t left = ptr - memLong_;
		str[left] = str[left + 1] = 's';
		left += 2;
		size_t right = left + getLongCount_(*(ptr + 1));
		for (size_t j = left; j < right; ++j)
		{
			str[j] = 'u';
		}
		ptr = reinterpret_cast<size_t*>(*ptr);
	}
	return str;
}