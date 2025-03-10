#pragma once

#include <vector>
#include <optional>
#include <stdexcept>


template<typename T>
struct HashNode final {
	std::string key{};
	T data{};

	HashNode<T>* next{nullptr};
};

template<typename T>
class HashTable final {
public:
	HashTable(size_t size) : table{size, nullptr} { }
	~HashTable();

	void insert(const std::string& key, const T& val);

	void remove(const std::string& key);

	bool contains(const std::string& key) const;

	const T& get(const std::string& key) const;
	const T& operator[](const std::string& key) const;

private:
	std::vector<HashNode<T>*> table{};

	size_t hash(const std::string& key) const;
};

template<typename T>
HashTable<T>::~HashTable()
{
	for (auto item : table) {
		auto temp = item;
		item = item->next;
		delete temp;
	}
}

template<typename T>
void HashTable<T>::insert(const std::string& key, const T& val)
{
	if (!contains(key)) {
		const size_t pos = hash(key);
		auto* newItem = new HashNode{key, val, table[pos]};
		table[pos] = newItem;
	}
	else {
		const size_t pos = hash(key);
		auto* it = table[pos];
		while(it) {
			if (it->key == key) {
				it->data = val;
				return;
			}
			it = it->next;
		}
	}
}


template<typename T>
const T& HashTable<T>::get(const std::string& key) const
{
	if (!contains(key)) {
		throw std::runtime_error("NOT IN THE MAP");
	}

	const size_t pos = hash(key);

	auto* it = table[pos];
	while (it) {
		if (it->key == key) { return it->data; }
	}
}

template<typename T>
bool HashTable<T>::contains(const std::string& key) const
{
	const size_t pos = hash(key);

	auto* it = table[pos];
	while (it) {
		if (it->key == key) { return true; }
	}

	return false;
}

template<typename T>
void HashTable<T>::remove(const std::string& key)
{
	const size_t pos = hash(key);

	auto* prev = table[pos];
	if (!prev) return;

	if (prev->key == key) {
		table[pos] = prev->next;
		delete prev;
		return;
	}

	auto* it = prev->next;

	while (it) {
		if (it->key == key) {
			prev->next = it->next;
			delete it;
			return;
		}

		prev = prev->next;
		it = it->next;
	}
}

template <typename T>
const T& HashTable<T>::operator[](const std::string& key) const
{
	return get(key);
}

template <typename T>
size_t HashTable<T>::hash(const std::string& key) const
{
	return std::hash<std::string>{}(key) % table.size();
}
