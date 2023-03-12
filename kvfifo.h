#ifndef KVFIFO_H
#define KVFIFO_H

#include <list>
#include <map>
#include <memory>
#include <utility>
#include <iterator>
#include <ranges>
#include <type_traits>

template<typename K, typename V>
requires std::totally_ordered<K> && std::regular<K> && std::copy_constructible<V>
class kvfifo {
private:
    using list_iterator = typename std::list<std::pair<K, V>>::iterator;
    struct data {
        std::map<K, std::list<list_iterator>> keys;
        std::list<std::pair<K, V>> values;
        constexpr static auto create_view(const auto &values) noexcept {
            return values | std::views::transform([](const auto &p) { return p.first; });
        }

        decltype(create_view(keys)) keys_view = create_view(keys);
    };

    std::shared_ptr<data> ptr_data;

    static std::shared_ptr<data> empty_data() {
        static const auto empty = std::make_shared<data>();
        return empty;
    }

    bool external_refs = false; //in

    void copy() {
        if (ptr_data.unique()) {
            return;
        }
        try {    	        
		    kvfifo copy = kvfifo();
		    for (auto kv : ptr_data->values) {
		        copy.push(kv.first, kv.second);
		    }
		    *this = copy;
		} catch(...) {
			throw;
		}
    }

public:
    using k_iterator = decltype(std::declval<data>().keys_view.begin());

    kvfifo() : ptr_data(std::make_shared<data>()),
                        external_refs(false) {}

    kvfifo(kvfifo const & other) : ptr_data(other.ptr_data) {
        if (other.external_refs) copy();
    }

    kvfifo(kvfifo && other) noexcept : ptr_data(std::move(other.ptr_data)),
                                       external_refs(std::move(other.external_refs)) {
        other.ptr_data = empty_data();
    }

    kvfifo &operator=(kvfifo other) {
        ptr_data = other.ptr_data;
        external_refs = false;
        return *this;
    }

    k_iterator k_begin() const {
        return ptr_data->keys_view.begin();
    }

    k_iterator k_end() const {
        return ptr_data->keys_view.end();
    }

    void push(K const &k, V const &v) {
       	bool values_pushed = false, flag = false;
        copy();
        
        try {
        	ptr_data->values.push_back({k, v});
        	values_pushed = true;
        	list_iterator it = prev(ptr_data->values.end());
        	
        	(ptr_data->keys)[k];
        	flag = true;
        	(ptr_data->keys)[k].push_back(it);
			
        	external_refs = false; //modyfikacja unieważnia referencje
        } catch (...) {
        	if(values_pushed == true) {
        		ptr_data->values.pop_back();
        	}
        	if(flag == true) {
        		if((ptr_data->keys)[k].size() == 0) {
        			(ptr_data->keys).erase(k);
        		}
        	}
            throw;
        }
    }

    void pop() {
        if (empty()) {
            throw std::invalid_argument("fifo is empty");
        }
        copy();
        
        auto element = ptr_data->values.front();
		ptr_data->values.pop_front();
		ptr_data->keys[element.first].pop_front();
		
		if(ptr_data->keys[element.first].size() == 0) {
			ptr_data->keys.erase(element.first);
		}

    	external_refs = false; //modyfikacja unieważnia referencje
    }

    void pop(K const &k) {
        if (count(k) == 0) {
            throw std::invalid_argument("no values asigned to given key");
        }
        copy();
        
    	list_iterator it = ptr_data->keys[k].front();
    	ptr_data->keys[k].pop_front();
    	ptr_data->values.erase(it);

		if(ptr_data->keys[k].size() == 0) {
			ptr_data->keys.erase(k);
		}

    	external_refs = false; //modyfikacja unieważnia referencje
    }

    void move_to_back(K const &k) {
        size_t size = count(k);
        if (size == 0) {
            throw std::invalid_argument("no values asigned to given key");
        }
        copy();
        
	    std::list<list_iterator> to_move = ptr_data->keys[k];
	    for (auto it : ptr_data->keys[k]) {
	        ptr_data->values.splice(ptr_data->values.end(), ptr_data->values, it);
	    }

	    external_refs = false; //modyfikacja unieważnia referencje     
    }

    std::pair<K const &, V &> front() {
        if (empty()) {
            throw std::invalid_argument("fifo is empty");
        }
        copy();
        external_refs = true;
        return {ptr_data->values.front().first, ptr_data->values.front().second};
    }

    std::pair<K const &, V const &> front() const {
        if (empty()) {
            throw std::invalid_argument("fifo is empty");
        }

        return {ptr_data->values.front().first, ptr_data->values.front().second};
    }

    std::pair<K const &, V &> back() {
        if (empty()) {
            throw std::invalid_argument("fifo is empty");
        }
        copy();
        external_refs = true;
        return {(*prev(ptr_data->values.end())).first, (*prev(ptr_data->values.end())).second};
    }

    std::pair<K const &, V const &> back() const {
        if (empty()) {
            throw std::invalid_argument("fifo is empty");
        }
        return {(*prev(ptr_data->values.end())).first, (*prev(ptr_data->values.end())).second};
    }

    std::pair<K const &, V &> first(K const &k) {
        if (count(k) == 0) {
            throw std::invalid_argument("no values asigned to given key");
        }
        copy();
        external_refs = true;
        auto it = ptr_data->keys.find(k)->second;
        return {(*it.front()).first, (*it.front()).second};
    }

    std::pair<K const &, V const &> first(K const &k) const {
        if (count(k) == 0) {
            throw std::invalid_argument("no values asigned to given key");
        }
        auto it = ptr_data->keys.find(k)->second;
        return {(*it.front()).first, (*it.front()).second};
    }

    std::pair<K const &, V &> last(K const &k) {
        if (count(k) == 0) {
            throw std::invalid_argument("no values asigned to given key");
        }
        copy();
        external_refs = true;
        auto it = ptr_data->keys.find(k)->second;
        return {(*it.back()).first, (*it.back()).second};
    }

    std::pair<K const &, V const &> last(K const &k) const {
        if (count(k) == 0) {
            throw std::invalid_argument("no values asigned to given key");
        }
        auto it = ptr_data->keys.find(k)->second;
        return {(*it.back()).first, (*it.back()).second};
    }

    size_t size() const noexcept {
        if (!ptr_data) {
            return 0;
        }
        return ptr_data->values.size();
    }

    bool empty() const noexcept {
        if (!ptr_data) {
            return true;
        }
        return ptr_data->values.empty();
    }

    size_t count(K const &x) const noexcept{
        if (!ptr_data || !ptr_data->keys.contains(x)) {
            return 0;
        }
        return ptr_data->keys[x].size();
    }

    void clear() noexcept {
    	if(empty()) {
			return;
    	}
      	copy();
      	ptr_data = std::make_shared<data>();
    }

};
#endif
