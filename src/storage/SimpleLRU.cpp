#include "SimpleLRU.h"
#include <iostream>

namespace Afina {
namespace Backend {

bool SimpleLRU::_put_anyway(const std::string &key, const std::string &value) {
    size_t ovr_size = key.size() + value.size();

    //case storage is empty
    if (_lru_head == nullptr and _lru_tail == nullptr) {
        if (ovr_size <= _max_size) {
            auto new_node = new lru_node{key, value, nullptr, nullptr};
            auto ptr = std::shared_ptr<lru_node>(new_node);
            _lru_head = ptr;
            _lru_tail = ptr;
            _lru_index.insert({std::cref(ptr->key), std::ref(*new_node)});
            _cur_size += ovr_size;
            return true;
        } else {
            return false;
        }
    }

    if (ovr_size <= _max_size) { // case we can insert
        while (_cur_size + ovr_size > _max_size) { // delete lru while there is not enough space
            const std::string key_to_del = _lru_head->key;
            std::string val_to_del = _lru_head->value;

            _lru_index.erase(key_to_del);
            _cur_size -= key_to_del.size() + val_to_del.size();

            if (_lru_head->next != nullptr) {
                std::shared_ptr<lru_node> new_head = _lru_head->next;
                _lru_head = new_head;
                _lru_head->prev = nullptr;
            } else {
                _lru_head = nullptr;
                _lru_tail = nullptr;
            }
        }
        auto new_node = new lru_node{key, value, nullptr, nullptr}; //inserting
        auto ptr = std::shared_ptr<lru_node>(new_node);
        _lru_tail->next = ptr;
        ptr->prev = _lru_tail;
        _lru_tail = ptr;

        _lru_index.insert({std::cref(ptr->key), std::ref(*new_node)});

        _cur_size += ovr_size;
        return true;
    } else { // case inserting is impossible
        return false;
    }

}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    auto search = _lru_index.find(key);
    if (search == _lru_index.end()) {
        return _put_anyway(key, value);
    } else {
        return Set(key, value);
    }

}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto search = _lru_index.find(key);
    if (search == _lru_index.end()) {
        return _put_anyway(key, value);
    } else {
        return false;
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto search = _lru_index.find(key);
    if (search != _lru_index.end()) { //key found
        lru_node& node_found = search->second.get();

        if (node_found.next != nullptr) {// moving recently used to the tail
            auto ptr = node_found.next->prev;
            ptr->next->prev = ptr->prev;
            if (ptr->prev != nullptr) {
                ptr->prev->next = ptr->next;
            } else {
                _lru_head = ptr->next;
            }
            ptr->next = nullptr;
            ptr->prev = _lru_tail;
            _lru_tail->next = ptr;
            _lru_tail = ptr;
        }


        if (node_found.key.size() + value.size() > _max_size) { //checking size
            return false;
        }

        while (_cur_size + value.size() - node_found.value.size() > _max_size) { //deleting lru
            const std::string key_to_del = _lru_head->key;
            std::string val_to_del = _lru_head->value;

            _lru_index.erase(key_to_del);
            _cur_size -= key_to_del.size() + val_to_del.size();

            if (_lru_head->next != nullptr) {
                std::shared_ptr<lru_node> new_head = _lru_head->next;
                _lru_head = new_head;
                _lru_head->prev = nullptr;
            } else {
                _lru_head = nullptr;
                _lru_tail = nullptr;
            }
        }
        node_found.value = value;

        return true;

    } else { // key not found
        return false;
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto search = _lru_index.find(key);
    if (search != _lru_index.end()) {
        lru_node& node_to_del = search->second.get();
        auto value = node_to_del.value;
        _lru_index.erase(key);
        _cur_size -= key.size() + value.size();
        if (node_to_del.prev == nullptr && node_to_del.next == nullptr) { // case head == tail
            _lru_tail = nullptr;
            _lru_head = nullptr;
        } else if (node_to_del.prev == nullptr) { //case deleting head
            std::shared_ptr<lru_node> new_head = _lru_head->next;
            new_head->prev = nullptr;
            _lru_head = new_head;
        } else if (node_to_del.next == nullptr) { //case deleting tail
            std::shared_ptr<lru_node> new_tail = _lru_tail->prev;
            new_tail->next = nullptr;
            _lru_tail = new_tail;
        } else {
            std::shared_ptr<lru_node> prev = node_to_del.prev;
            std::shared_ptr<lru_node> next = node_to_del.next;
            prev->next = next;
            next->prev = prev;
        }
        return true;
    } else { // key not found
        return false;
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto search = _lru_index.find(key);
    if (search != _lru_index.end()) {
        lru_node& node_found = search->second.get();
        value = node_found.value;

        if (node_found.next != nullptr) { //moving recently used element to the tail
            auto ptr = node_found.next->prev;
            ptr->next->prev = ptr->prev;
            if (ptr->prev != nullptr) {
                ptr->prev->next = ptr->next;
            } else {
                _lru_head = ptr->next;
            }
            ptr->next = nullptr;
            ptr->prev = _lru_tail;
            _lru_tail->next = ptr;
            _lru_tail = ptr;
        }


        return true;
    } else { //key not found
        return false;
    }
}

} // namespace Backend
} // namespace Afina
